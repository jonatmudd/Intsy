
%% script to convert Intsy file to GEMS format
% Created JE 04 May 2023 for IBS study
%
% Usage:
% 1. Update data file path
%
%
% 3. Update resamp.active option
%
% 4. Run the script!

%% define filepath
[COMP_ID, boxroot, wrsroot] = boxrootdirIBS2023();
expfolder = 'ST_001';
fname = 'Intsy_90.tsy';

datadir = fullfile(wrsroot, 'DataFiles', expfolder );
fpath = fullfile(datadir, fname);

%% Data load options

% band stop filter
bsf.active = false;

% resamp opts
resampOpt.active = false;
resampOpt.efs = 4; % efs is  effective sampling freq , only applies if resamp.active = true

% write GEMS file (MATLAB GUI)
gemsfile.write = true;
if resampOpt.active
    gemsfile.suffix = '_GEMS_resamp4Hz.mat';
else
    gemsfile.suffix = '_GEMS.mat';
end

%% actually load intsy file converted to matlab format
% will make and save GEMS file if gemsfile.write = true;
[Data, Errsync] = intsy128ch_dataload(fpath, bsf, gemsfile, resampOpt);



