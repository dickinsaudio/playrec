% A set of small tests to give playrec a bit of a workout

%% A loopback loop
playrec('open','Device',4,'In',2,'Out',2,'Buffer',96000);
T=playrec('now');
N=128;
D=256;
while(1)
    in = playrec('record',T-N,N);
    playrec('play',T+D,in);
    T = T+N;
    drawnow;
    while(T>playrec('now')) drawnow; end;
end;



%% Play and capture random length pieces that should make a perfect tone

playrec('open','Device',4,'In',2,'Out',2,'Buffer',96000);
t = (1:5*48000)';
tone = 0.1*sin((t/48000)*2*pi*100*sqrt(2))*[1 1];
tone = int32(2^31*tone);
y    = 0*tone;
n    = 0;
T    = playrec('now')+2000;
s    = 1000;

while (n+s<length(tone))
    playrec('play',T,tone(n+(1:s),:));
    n = n+s;
    T = T+s;
    s = ceil(2000*rand(1));
    while (playrec('now')<T-2000) pause(0.001); end;
end;
x = playrec('record');



%% Look at the input and output skew, and see if it is independent of block size
%  Constant at 64 for PCIe with loop back direct, 
%  164 with loopback via a 1ms BKII I2S loopback
%  etc

playrec('open','Device',4,'In',2,'Out',2,'Buffer',96000);
x = 2^31*int32((1:2000 == 500)'*[1 1]);
playrec('play',48000,x);
while (playrec('now')<1.2*48000) pause(0.001); end;
y = playrec('record',48000,length(x));
plot([x y]);
[mx at] = max([x y]);
fprintf("HWBuffer %d   Latency %d\n",playrec('hwBuffer'),at(3)-at(1));










