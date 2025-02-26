function X = resampdata (S, nativesr, efs, decfac)

% X = resampdata (S, nativesr, efs, decfac)
% resamples the signals in S from the native sampling rate to a desired effective sampling rate.
% The resampling process involves 2 steps, an upsampling followed by decimation
%
% S [Nchans x Nsamps] data matrix in rows
% nativesr   native sampling rate (Hz). This typically the experimental recording sampling frequency
% efs   desired sampling rate (Hz).  For GI Slow waves, we typically like 30 Hz
% decfac   parameters used to mesh these two step resampling. default value is 10
%
% Created: 03 mar 2017 JE


if nargin< 4 | isempty(decfac)
    decfac = 10;
end

%in resamples(S, p, q) p must be > q. Hence, two steps for
%resampling, then decimation
[Nchans, Nsamps] = size(S);

% resampledrate = efs*decfac
% nativesr

%now loop over the rest of the channels
for n = 1:Nchans  %have to use for loop for decimate() function, doesn't operate on matrix
    
    Xtmp = resample(S(n,:), efs*decfac, nativesr);
    X(n,:) = decimate(Xtmp, decfac);
end