function [Data, ErrSync] = intsy128ch_dataload(fpath, bsf, gemsfile, doResamp, dataformat)

% [Data, Errsync] = intsy128ch_dataload(fpath, bsf, gemsfile, resamp, dataformat)
%  Loads Teensy-Intan binary data file recorded via LabView interface to
%  the 128 channel Ambulatory Intan-Teensy (Intsy) module + 3 channels of
%  accelerometer readings (ADXL 335) + Vdd (voltage supply to intan rhd2000)
%  + Vflex (flex sensor voltage)
% 
%  GEMS compatible file is optionally created for further off-line
%  analysis.  Data can be optionally band pass filtered to remove power
%  line noise.
%
%  User may optionally truncate file based on position of first sync error.
%  For example, a 4-hr file may be allocated, but we only record for 3 hr,
%  then turn off power to the system generating a sync character error
%  indicative of power turning off.  
%
%  Diagnostic figure shows 3 hallmark indicators of recording integrity: 
%   1. Timestamp (s) vs. sample number - should be perfectly linear
%   2. Vdd for Intan chips vs. time should be 3.0 - 3.3V
%   3. Acceleration in z coordiante vs. time (should be fairly stable,
%       indicate behavioral movement, e.g. sitting up to eat a meal
% 
%  NB: This is intended for loading data acquired with the 128 channel
%  system (first tests conducted June 2019). 
%
%   Use teensy_intan_dataload() for  data files acquired with the the 
%        original 32 chan system circa 2016 Aug - May 2017.  
%
%  Use intsy64_dataload() for data acquired with 64 chan non-ambulatory hardware/firmware version.
%
%  Use intsyAmbulatory_dataload() for data acquired with v1 of 64 chan
%  ambulatory system circa Dec 2018-Apr 2019
%
%
% ------- OUTPUTS -------
% Data:  structure with fields:
%
%     Y             Intan data signals [Nrows x Nsamps]
%     timestamp     microsecond time stamps at which each hardware conversion was triggered (ie when data points were sampled)
%     dt            intersample sampling interval (us)
%     FS            sampling rate mean (Hz)
%     tvec          perfectly periodic version of timestamp output, based on mean sampling rate FS.
%     accelvolts    accelerometer data in units of volts
%     accelg        accelerometer readings in units of g
%     Vdd           voltage at power supply to Intan RHD2132 chip 
%                    (should always be ~3.2 - 3.3 V for normal function0
%     Vflex         voltage of flex sensor in units of volts
%
% Errsync   structure with fields ii, jj, qq
%             ii, jj are indices of points which returned improper aux_cmd result. This should be 'I' and 'N' (from INTAN)
%             qq  indices of data points which had hardware timing inaccuracy of > 1 us.
%
%            Note that the time at which errors occurred can be gotten
%            with: >> badsamps = Data.tvec(Errsync.qq)
%
% -----     INPUTS -------
% fpath: full path to Intan Teensy binary file (recorded with Labview)
%
% bsf: band stop filter options, a structure with fields:
%                 active: boolean specifies whether to do bsf or not(default: false)
%                 lowcut: low cut(Hz).  Typically ~47  Hz to match power line
%                                   noise (may be 60 hz in US)
%                 highcut:  high cutoff (Hz).  Typically ~53 Hz for 50 Hz mains.
%
% gemsfile:  structure with 2 fields:
%               write: boolean flag to write GEMS compatible file (default: false)
%               suffix: file suffix for automatically generated file path. (default: '_GEMS.mat')
%
% dataformat: structure defining how data blocks are organized.  This is defined by intsy firmware version used at time of 
%               experiment and MUST must be consistently defined below. The
%               defaults were set according to 08 july 2019 firmware
%               updates adding flex sensor.
% 
%              Nwords:  number of words per data block. Default = 144.
%                       It should always be set to 143 for experiments prior to July
%                       08 2019.  (Flex sensor was added after this time increasing
%                       data block to 144 words)
%              
%              Noffset: number of words signals are offset.  It should be always
%              set for 10 for now (2 words timestamp + 4*2 words for aux_cmd result)
%              Equivalently, this is 20 bytes.
%
%             Nchans: number of channels recorded.  Should always be 128 for
%             now (ambulatory device, starting 25 Sept 2018)
%
%             Naccel: accelerometer channels recorded. Should always be 3
%                     (X,Y,Z components)
% 
%             Nflex:  number of flex sensors. default = 1. 
%                         Set dataformat.Nflex = 0 for files acquired prior to July 08
%                          2019 (when flex sensor hardware/ firmware was upgraded
% 
%             MagicNum: Intsy 'magic number' verifying Teensy synced with
%                       SD card writes.  As of 02 July this should be 8481
%                       (= 0x2121; = '!!' in ascii). Prior to this time it was
%                       22379.
%
% 
%  doResamp: structure defining resampling parameters
%            .active:  boolean specifies whether to resample or not. Default = false;
%            .efs:  desired sampling rate after resampling in Hz.
%             Note that resampling can be done at a later time.  See also resampdata.m
%
%
%
% Created: Jon Erickson, 15 June 2019.
% Last modified: 
%          - JE 08 july 2019: added flex sensor option to truncate
%               files where first sync error occurs.
%
%          -JE 05 Nov 2019: limiting FS calculation to using only valid
%                          samples---corrects issue where we may only have
%                          say 2 hrs of valid recording in a SD file set
%                          for 20 hrs.
%         - JE 19 Nov 2020: added heuristic to get Vdd and Vflex row order
%                            properly done for newly fab'd CDAB module
%                           upgraded error checking for case where pcb A
%                           slot is not occupied.
%                
%  
% See also resampdata

if nargin <2 || isempty(bsf)
    bsf.active = false;
end

if nargin < 3 || isempty(gemsfile)
    gemsfile.write = false;
end

if nargin < 4 || isempty(doResamp)
    doResamp.active = false;
end

% for 128ch Intan system as of June 07 2019
if nargin < 5 || isempty(dataformat)
    dataformat.Nwords = 144; 
    dataformat.Noffset = 10;
    dataformat.Nchans = 128;
    dataformat.Naccel = 3;
    dataformat.Nflex = 1; %should set to 0 for files acquired prior to july 08 2019
    dataformat.MagicNum = 8481;
end

%% Define 32 bit clock roll over
T_ROLLOVER = 2^32 - 1; % for 32 bit counter = 4294967295


%% open binary data file

%check to make sure it exists first.  If not, return empty
FileExists = exist(fpath, 'file');
if FileExists==0
    warndlg(sprintf('File requested does not exist: %s.\n  Check data directory, file name, and extension, then try again.', fpath))
    Data = [];
    ErrSync = [];
    return
end


fprintf('Opening file: %s\n', fpath)

fid = fopen(fpath);

Nsamples = inf; %load the entire data file.

fprintf('Reading file: %s\n', fpath)
ydat2 = uint16(fread(fid, [dataformat.Nwords, Nsamples], 'uint16')); %143 words (=286 bytes) written per data chunk
ydat2(:,1) = []; %First read is always junk, cmd_pipeline offset not established.

%% First 2 words should contain 73 and 78 'I' and 'N' in INTAN for amp A
%  Next 2 words should contain  84 and 65  'T'  and 'A' in INTAN for amp B
% these should be empty.  If not, SPI link was not working properly
iiA = find(ydat2(1,:) ~=73);
jjA = find(ydat2(2,:) ~=78);
iiB = find(ydat2(3,:) ~=84);
jjB = find(ydat2(4,:) ~=65);

iiC = find(ydat2(5,:) ~=73);
jjC = find(ydat2(6,:) ~=78);
iiD = find(ydat2(7,:) ~=84);
jjD = find(ydat2(8,:) ~=65);

iiSD = find(ydat2(dataformat.Nwords,:) ~= dataformat.MagicNum);

% if (~isempty(iiA) | ~isempty(jjA) | ~isempty(iiB) | ~isempty(jjB))
%     warndlg('Data sync IN characters are misread at some time points! See errsync output for more info')
% end
ErrSync.iiA = iiA; % amp A
ErrSync.jjA = jjA;
ErrSync.iiB = iiB; % amp B
ErrSync.jjB = jjB;
ErrSync.iiC = iiC; % amp C
ErrSync.jjC = jjC;
ErrSync.iiD = iiD; % amp D
ErrSync.jjD = jjD;

ErrSync.iiSD = iiSD; %Teensy generated sync bit to check SD card writes.

 

%% Next 2 words form timestamp in high and low byte packet
%Compute time diffs between sample, identify any that did not conform to
%DTsamp (microsec)

%timestamp is 4 bytes, have to boolean operate together 2 byte reads
%(uint16).
% Note that first 2 words are result of aux_cmds, hence indices of 6 and 5
% below to get timestamp).  Input of 16 comes from 2 bytes = 16 bits
timestampRaw = bitor(bitshift(double(ydat2(dataformat.Noffset,:)), 16 ), double(ydat2(dataformat.Noffset-1,:) ));

%unwrap any clock rollover.  0.99x factor is heuristic, could also occur at
%a frame misread, but very unlikely.  In any case, user has access to raw
%timestamps to do what they wish.
timestamp = unwrap(timestampRaw, 0.999*T_ROLLOVER);

timestamp = timestamp - timestamp(1);  %shift time origin to 0

dt = diff(timestamp);
DTmedian = median(dt); %use median instead of mean since one 'bad' sample can really throw off calculation.
timeTolMicrosec = 1;  %hardware timing tolerance
qq = find(abs(dt-DTmedian) > timeTolMicrosec);

%% valid time indices are those for which neither amp A or C reports errors
% FS computation based on valid time indices.
% This is done because a data file set for say 20 hours may only have 4
% hours or valid recording time.  Thus, we want only valid samples to be
% used when computing FS.
% validTimeIndex = setdiff(1:length(timestamp), union(ErrSync.iiA, ErrSync.iiB));
validTimeIndex = setdiff(1:length(timestamp), intersect(intersect(ErrSync.iiA, ErrSync.iiB), intersect(ErrSync.iiC, ErrSync.iiD)));
whos validTimeIndex
validTimeIndex(end) = []; 

FS = 1e6/median(dt(validTimeIndex));



ErrSync.qq = qq;


%% check if user wants to truncate file based on where first sync error occurred
errMinIndex = min(iiA); %first index at which sync error is detected
errMinIndex = min(validTimeIndex(end)); %first index at which sync error is detected
errTsec = errMinIndex/FS; %time in seconds at which first sync error was detected
errTminutes = errTsec/60; %time in minutes at which first sync error was detected
truncateOpt = false; %default is to NOT truncate
%
if ~isempty(errTminutes)
    answer = questdlg(sprintf('Truncate file at %2.1f min?', errTminutes), ...
	'Truncate File Option', ...
	'Yes','No', 'No');
% Handle response
switch answer
    case 'Yes'
        truncateOpt = true;
        
    case 'No'
        truncateOpt = false;
    
end
end
%% get the ADC data. Offset +4 index (dataformat.Noffset) is to skip over
% first 6 Uint16's which are 2*2 (=4) aux_cmd_results and 32-bit timestamp

% Optionally truncate data file just before first sync error detected
% This typically occurs when the power is turned off; e.g. we set a 4 hr
% recording, but only actually record for 3 hrs, then turn system off 1 hr
% early.
if truncateOpt
    lastValidIndex = errMinIndex-10*FS; %equiv to removing 10 s worth of points: Teensy writes ever 10 s to SD 
    data = ydat2((1+dataformat.Noffset):end, 1:lastValidIndex);
    timestamp = timestamp(1:lastValidIndex);
    dt = dt(1:lastValidIndex);
else
    data = ydat2((1+dataformat.Noffset):end, :);
end

%copy into outputs
Data.timestamp  = timestamp;
Data.dt = dt;
Data.FS = FS;

%loop over each channel
[Nsigs, Nsamps] = size(data);

%inform user of recording duration
Duration.Samps = Nsamps;
Duration.Sec = Nsamps/FS;
Duration.Min = Duration.Sec/60;
Duration.Hrs = Duration.Min/60;
% fprintf(sprintf('Num Samps, Valid Secs: %4.4d, %4.4d\n', Duration.Samps, Duration.Sec));
fprintf(sprintf('Recording Duration: %4.1d min = %2.1f hr\n', round(Duration.Min), Duration.Hrs));

if dataformat.Nflex==0 %no flex sensor
    accelRowIndex = (Nsigs-dataformat.Naccel - 1): (Nsigs-2);
    VddRowIndex = Nsigs - 1;
    flexRowIndex = [];
else %flex sensor installed
    accelRowIndex = (Nsigs-dataformat.Naccel - 2): (Nsigs-3);
    VddRowIndex = Nsigs - 2;
    flexRowIndex = Nsigs - 1;
end
acceldata = data(accelRowIndex, :); %acceleromter data

%scale accelerometer data
accelvolts = double(acceldata)*(3.3/2^16);  % Teensy 3.6 operates as 3.3 volt device reading 16 bits

accelg = (accelvolts-1.65)/0.3; %adxl335 acceleromter datasheet: 1.5V calibrated to be 0g and 1g/300 mV.
Data.accelg = accelg;
Data.acceldata = acceldata;
Data.accelvolts = accelvolts;

%% Vbattery (Vdd voltage supply for intan amp)
Vbatdata = data(VddRowIndex, :); %Vdd data from Intan amp

% the Intsy CDAB module made nov 19 2020 has firmware which revesed the
% order of Vflex and Vdd, heuristic to check for this:
if all(Vbatdata==0)
    flipVdd_vflex = true;
    Vbatdata = data(VddRowIndex+1,:);
else
    flipVdd_vflex = false;
end
Vbatvolts = double(Vbatdata)*7.48E-5;
Data.Vdd = Vbatvolts;

%% Vflex (flex sensor reading)
if dataformat.Nflex==1
    if flipVdd_vflex %check to see if we have firmware that flipped rows of Vdd and Vflex
        flexRowIndex = flexRowIndex-1; %
    end
    VflexBin = data(flexRowIndex ,:);
    Vflexvolts = double(VflexBin)*(3.3/2^16);
    Data.Vflex = Vflexvolts;
else %no flex sensor attached fill in with nan as place holder
    Data.Vflex = nan(1,length(Vbatvolts));
end

%% scale the Intan data from uint16 binary to uV. See pg 5 of:
%  http://intantech.com/files/Intan_RHD2000_data_file_formats.pdf
% which specifies conversion factor
IntanDataRaw = data(1:dataformat.Nchans, :); %electrical signal data only (last 3 chans are accelerometer)
scaled_data = (double(IntanDataRaw) - 2^15) * 0.195;

tvec = DTmedian*1e-6*[1:size(scaled_data,2)];
Data.tvec = tvec;


%% make band pass filter and high pass filter
if ~bsf.active
    Data.Y = scaled_data;
    
    
else %band stop filter before returning result
    %     2nd order filter applied to fc +/- bw
    
    Yfilt = nan(size(scaled_data));
    
    for mm = 1:length(bsf.lowcut)
        filtlims = [bsf.lowcut(mm), bsf.highcut(mm)]/(FS/2);
        
        % check if filter cutoffs are valid
        
        filtnotvalid = find(filtlims > 1);
        if isempty(filtnotvalid) %all good, do the filter
            [b,a] = butter(2, filtlims, 'stop');
            
            %             %have to loop to use filt filt, pre-allocate output for speed
            %             Yfilt = nan(size(scaled_data));
            %
            for n = 1:size(Yfilt,1)
                Yfilt(n,:) = filtfilt(b,a,scaled_data(n,:));  %band stop filter to get rid of power line
            end
        else %filter cutoff are out of bounds.
            warndlg('Filter not applied since requested band stop filter limits exceed valid bounds of (0, FS/2)\n. \t Returning scaled data (not filtered)\n')
            Data.Y = scaled_data;
        end
    end
    
    Data.Y = Yfilt; %output band stop filtered data
    
end

%% optionally resample data

if doResamp.active
    Data.Y = resampdata (Data.Y, Data.FS, doResamp.efs);
end

%% create GEMS compatibile file (optional)

if gemsfile.write
    gemsfile
    ElecData = Data.Y;
    %set proper sampling rate in GEMS file
    if ~doResamp.active
        fs = Data.FS;
    else
        fs = doResamp.efs;
    end
    resamp = false;
    physdim = repmat('uV',dataformat.Nchans,1);

    
    %default suffix for GEMS file
    if ~isfield(gemsfile, 'suffix')
        gemsfile.suffix = '_GEMS.mat';
    end
    
    [pname, fname, fext] = fileparts(fpath);
    gemspath = fullfile(pname, [fname, gemsfile.suffix]);
    save(gemspath, 'ElecData', 'fs', 'physdim', 'resamp');

    fprintf('Saved GEMS compatible file to: %s \n', gemspath);
    
end

fclose('all');

%% plot a diagnostic figure
figure;

subplot(3,1,1); %timestamp
plot(Data.timestamp/1e6, 'k');
xlabel('Integer Sample Number'); ylabel('Timestamp (s)')
title('Intsy File diagnostics')
set(gca, 'fontsize', 16)

subplot(3,1,2); % battery voltage vs. time
plot(Data.timestamp/1e6, Data.Vdd);
ylim([1 5]); 
xlabel('Time (s)'); 
ylabel('V_{dd} (V)', 'interpreter', 'tex')
set(gca, 'fontsize', 16)

subplot(3,1,3); % z-accel vs. time
plot(Data.timestamp/1e6, Data.accelg(3,:), 'r');
xlabel('Time (s)'); ylabel('accel-z (g)');
set(gca, 'fontsize', 16)

set(gcf, 'color', 'w');
set(gcf, 'units', 'normalized', 'position', [0.0063    0.0537    0.2945    0.8722]); 
end