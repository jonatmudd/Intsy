function [Data, Errsync] = intsy64_dataload(fpath, bsf, gemsfile, doResamp, dataformat)

% [Data, Errsync] = intsy64_dataload(fpath, bsf, gemsfile, resamp, dataformat)
%  loads Teensy-Intan binary data file recorded via LabView interface to
% the 64 channel Intan-Teensy (Intsy) module.
%  GEMS compatible file is optionally created for further off-line
%  analysis.  Data can be optionally band pass filtered to remove power
%  line noise.
%
%  NB: This is intended for laoding data acquired with the newer 64 channel
%  system (first test date 28 June 2017). Use teensy_intan_dataload() for
% data files acquired with the the original 32 chan system circa
%  2016 Aug - May 2017.

% -----     OUTPUTS -------
% Data:  structure with fields:
%
%     Y     Intan data signals [Nrows x Nsamps]
%     timestamp   microsecond time stamps at which each hardware conversion was triggered (ie when data points were sampled)
%     dt    intersample sampling interval (us)
%     FS     sampling rate mean (Hz)
%     tvec   perfectly periodic version of timestamp output, based on mean sampling rate FS.
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
% dataformat: structure defining how data blocks are organized.  This is defined in LabView software and must be consistent here
%              Nwords:  number of words per data block.  This is defined by Labview software.  It should always be set to
%              70 words (=140 bytes) per data block, unless notified otherwise
%             (for future compatibility with other data formats)
%
%              Noffset: number of words signals are offset.  It should be always
%              set for 6 for now (2 words timestamp + 2*2 words for aux_cmd result)
%              Equivalently, this is 12 bytes.
%
%             Nchans: number of channels recorded.  Should always be 64 for
%             now (v2 of the device as of June 28 2017)
%
% doResamp: structure defining resampling parameters
%            .active:  boolean specifies whether to resample or not. Default = false;
%            .efs:  desired sampling rate after resampling in Hz.
%             Note that resampling can be done at a later time.  See also resampdata.m
%
%
%
% Created: Jon Erickson, 20 apr 2017.
% See also resampdata, teensy_intan_dataload()

if nargin <2 || isempty(bsf)
    bsf.active = false;
end

if nargin < 3 || isempty(gemsfile)
    gemsfile.write = false;
end

if nargin < 4 || isempty(doResamp)
    doResamp.active = false;
end


if nargin < 5 || isempty(dataformat)
    dataformat.Nwords = 70;
    dataformat.Noffset = 6;
    dataformat.Nchans = 64;
end



% mypath = fullfile(datadir, myfile)
fid = fopen(fpath);

% Nsamples = 50000;

Nsamples = inf; %load the entire data file.

ydat2 = fread(fid, [dataformat.Nwords, Nsamples], 'uint16'); %36 words written per data chunk
ydat2(:,1) = []; %First read is always junk, cmd_pipeline offset not established.

%% First 2 words should contain 73 and 78 'I' and 'N' in INTAN for amp A
%  Next 2 words should contain  84 and 65  'T'  and 'A' in INTAN for amp B
% these should be empty.  If not, SPI link was not working properly
iiA = find(ydat2(1,:) ~=73);
jjA = find(ydat2(2,:) ~=78);
iiB = find(ydat2(3,:) ~=84);
jjB = find(ydat2(4,:) ~=65);

if (~isempty(iiA) | ~isempty(jjA) | ~isempty(iiB) | ~isempty(jjB))
    warndlg('Data sync IN characters are misread at some time points! See errsync output for more info')
end
Errsync.iiA = iiA; % amp A
Errsync.jjA = jjA;
Errsync.iiB = iiB; % amp B
Errsync.jjB = jjB;


%% Next 2 words form timestamp in high and low byte packet
%Compute time diffs between sample, identify any that did not conform to
%DTsamp (microsec)

%timestamp is 4 bytes, have to boolean operate together 2 byte reads
%(uint16).
% Note that first 2 words are result of aux_cmds, hence indices of 6 and 5
% below to get timestamp).  Input of 16 comes from 2 bytes = 16 bits
timestamp = bitor(bitshift(double(ydat2(6,:)), 16 ), double(ydat2(5,:) ));
timestamp = timestamp - min(timestamp);  %shift time origin to 0
dt = diff(timestamp);
DTmedian = median(dt); %use median instead of mean since one 'bad' sample can really throw off calculation.
timeTolMicrosec = 1;  %hardware timing tolerance
qq = find(abs(dt-DTmedian) > timeTolMicrosec);
FS = 1e6/median(dt);

%copy into outputs
Data.timestamp  = timestamp;
Data.dt = dt;
Data.FS = FS;

Errsync.qq = qq;


%% get the ADC data. Offset +4 index (dataformat.Noffset) is to skip over
% first 6 Uint16's which are 2*2 (=4) aux_cmd_results and 32-bit timestamp

%loop over each channel
data = ydat2((1+dataformat.Noffset):end, :);
% do we really need to loop?
% for k = 1:dataformat.Nchans
%     data(k,:) = ydat2(k + dataformat.Noffset , :);
% end

%% scale the data from uint16 binary to uV. See pg 5 of:
%  http://intantech.com/files/Intan_RHD2000_data_file_formats.pdf
% which specifies conversion factor
scaled_data = (data - 2^15) * 0.195;
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
end