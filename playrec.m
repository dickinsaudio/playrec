function [ Y out2 out3 out4 out5] = playrec(varargin)
% playrec   Simultaneous playback and capture of audio through an ASIO device
%
% Usage
% playrec('devices')        Return the number of devices
% [ name, in, out, rate, block] = playrec('device',n)       
%                           Return the name and additional details of the numbered device
%                           block is the underlying ASIO blocksize which can impact latency
%                           If in, out, and/or rate are 0, then the device may be unusable
%
% playrec(X, ...)           Play the vector X with values [-1..1] using the initial set of channels of the device
% Y = playrec(t, ...)       Capture t seconds of audio
% Y = playrec(X, ...)       Capture audio whilst playing the file X
%
% OPTIONS
%   'background', logical   Execute in the background - default is 0 (false)
%   'outputs', [ ... ]      Array of the channels to use (size is the number of columns of X)
%   'inputs', N             Number of inputs to capture - default is the size of the device
%   'inputs', [ ... ]       Map of the inputs channels to use
%   'delay', T              Delay from opening device to starting play - default 50ms
%

if (nargin<1) error("Insufficient arguments"); end;

X = varargin{1};
if (isstring(X) || ischar(X))
    if (strcmpi(X,'devices')) Y = playrec_engine('devices'); return;
    elseif (strcmpi(X,'device'))  [ Y out2 out3 out4 out5 ] = playrec_engine('device',varargin{2}); return; 
    else error('Unknown option'); end;
end;

p = inputParser;

addParameter(p, 'background', false, @islogical);
addParameter(p, 'device',     1,     @(x) (round(x)==x && x>0 && x<=playrec_engine('devices')) );
addParameter(p, 'outputs',    [],    @isnumeric);
addParameter(p, 'inputs',     [],    @isnumeric);
addParameter(p, 'delay',      0.05,   @isnumeric);

parse(p, varargin{2:end});
fields = fieldnames(p.Results);
for i = 1:numel(fields)
    fieldName = fields{i};
    fieldValue = p.Results.(fieldName);
    eval([fieldName ' = fieldValue;']);
end;

[ name, in, out, rate ] =  playrec_engine('device',device);

if (in==0 && out==0) error("Not a valid device"); end;

if (isscalar(X))       X = (zeros(round(rate*X),1)); end;
if (isempty(outputs))  outputs = 1:size(X,2); end;
if (isempty(inputs))   inputs  = in; end;
if (isscalar(inputs))  inputs  = 1:min(in,inputs); end;
if (background)        error("Background mode not yet supported"); end;

cleanupobj = onCleanup(@cleanup);

playrec_engine('open','Device',device,'In',max(inputs),'Out',max(outputs),'Buffer',96000);

d=delay*rate;
n=24000;
N=size(X,1);
Y=zeros(N,length(inputs));

x = zeros(n,max(outputs),'int32');

x(1:min(n,N),outputs) = int32(X(1:min(n,N),:)*2^31);
playrec_engine('play',d,x);
t = 0;

while(t<size(X,1))
    T = t + min(n,N-t);
    play = min(n,N-T);
    rec  = min(n,N-t);
    if (play>0)
        x(1:play,outputs) = int32(X(T+(1:play),:)*2^31);
        playrec_engine('play',d+T,x(1:play,:));
    end;
    while(playrec_engine() < d+t) pause(0.01); drawnow; end;
    y = playrec_engine('record',d+t,rec); 
    Y(t+(1:rec),:) = double(y(1:rec,inputs))/2^31;
    t = T;
%    fprintf("Time %4.1f of %4.1f\n",t/rate,N/rate);
end;

function cleanup()
    clear playrec_engine;

