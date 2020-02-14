clear;

global num_sources rotate_angle planar_layout;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Instructions to reproduce results in the paper
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% xy_res = 0.002;

%%% fig a
% num_sources = 1; rotate_angle = 0; planar_layout = 1;
%%% fig b
% num_sources = 1; rotate_angle = 0; planar_layout = 0;
%%% fig c
% num_sources = 9; rotate_angle = 0; planar_layout = 0;
%%% fig d
% num_sources = 1; rotate_angle = 15; planar_layout = 0;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Simulation settings
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

ref_dba_1m = 68.0; % dBA @ 1m
Pref = 10^(ref_dba_1m / 20) * 0.00002; % pascal @ 1m

% % number of independent jamming sources
% num_sources = 1;
% % rotate angle
% rotate_angle = 0;
% % choose planar or circular layout
% planar_layout = 0;

% resolution of x-axis and y-axis
xy_res = 0.001;
% time resolution, the number should not be divisible by 1/fs (fs = 25khz here)
t_res = 0.013573 / 2;

% speed of sound, set to 340m/s
C = 340.;

% random seed
seed = 0;

% x, y range to simulate (m)
xy_range = 1.11; 

% simulation duration (s)
sim_duration = 0.4;

%%%%%%%%%%%%%%%%%%
% speakers layout
%%%%%%%%%%%%%%%%%%

% height of speakers to the horizontal plane
height = 0;

% number of speakers
n_speakers = 9;

% Positions and orientations of speakers
% Each speaker is represented by (x, y, z, x0, y0, z0),
% where (x, y, z) is the position of a speaker,
% and (x-x0, y-y0, z-z0) is the direction the speaker pointing to (i.e. norm).
speakers = zeros(n_speakers, 6);

% radius of the jammer
radius = 0.11/2;

if planar_layout <= 0
    for i=1:9
        ang=360/9*i + 90;
        speakers(i,:) = [radius*cosd(ang)+0.1 radius*sind(ang) height 0.1 0 height];
    end
else
    for i=1:3
        for j=1:3
            x0 = 0.1+0.015*(j-2);
            z0 = 0.02*i + height;
            speakers((i-1)*3+j,:) = [x0 0.001 z0 x0 0 z0];
        end
    end
end

%%%%%%%%%%%%%%%%%%
% speakers signal
%%%%%%%%%%%%%%%%%%

rng(seed);

% jamming signal sampling frequency
fs = 96000;

% a chirp is a sine wave of a frequency randomly picked up within
% [min_freq, max_freq]
t_chirp = 0.00045; % 0.45ms
min_freq = 24000;
max_freq = 26000;

% calculating additional signal in the beginning
t_start = (xy_range+1) / C;
t_each_signal = t_start+sim_duration;

signals = [];

for i=1:num_sources
    signal = [];
    for j=1:round(t_each_signal/t_chirp)
        t = 0:1/fs:t_chirp;
        chirp_freq = randi([min_freq max_freq], 1, 1);
        chirp = sin(2*pi*chirp_freq*t);
        signal = [signal chirp];
    end
    signals = [signals; signal];
end

% Assign signal to each speaker
sig = [];
for i=1:n_speakers
    ii = mod(i-1, num_sources);
    sig = [sig; signals(ii+1, :)];
end

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

srange = 1:1:n_speakers;

xrange = 0:xy_res:xy_range;
yrange = xrange;

trange = t_start : t_res : t_start+sim_duration;

% Generate roration angle for each time step,
% assuming rotation speed is 15deg/0.45second
gesture_rotate = zeros(length(trange), 1);
if rotate_angle > 0
    for t=1:length(trange)
        gesture_rotate(t) = mod((t-1) * t_res * 15/0.45, rotate_angle);
    end
end

% final averaged power map over time
evened = zeros(length(yrange),length(xrange));
% signal amplitude of a single speaker at a single time
amps_speaker = zeros(length(yrange),length(xrange));

ct=1;
for t=trange
    % power map at time t
    amps = zeros(length(yrange),length(xrange));
    
    for s=srange
        cx=1;

        xs = speakers(s, 1);
        ys = speakers(s, 2);
        zs = speakers(s, 3);
        x0 = speakers(s, 4);
        y0 = speakers(s, 5);
        z0 = speakers(s, 6);
        
        % Applying rotation
        theta=gesture_rotate(ct,1);
        R = [cosd(theta) -sind(theta); sind(theta) cosd(theta)];
        xys = [x0 y0] + [xs-x0 ys-y0] * R;
        xs = xys(1);
        ys = xys(2);

        for x=xrange
            cy=1;
            for y=yrange               
                % angle between norm of the speaker and the 3D point
                u = [x-xs y-ys 0-zs];
                v = [xs-x0 ys-y0 zs-z0];
                d_ang = atan2(norm(cross(u, v)), dot(u, v));

                % Calculate distance between the speaker and the receiving
                % location
                dis = sqrt((x-xs)^2 + (y-ys)^2 + (0-zs)^2);
                
                % Based on the angle and distance
                % loss = directivity loss * propagation loss
                loss = loss_dir(d_ang) * loss_dis(dis, C);

                % Calculate signal amplitude received at a point (x, y)
                % at time t.
                % Signal amplitude at (x, y, t) 
                %    = loss * signal amplitude at the speaker at time (t-delay),
                % where delay = distance / sound of speed.
                amps_speaker(cy, cx) = get_amp(dis, s, t, C, sig, fs) * loss * Pref;

                cy=cy+1;
            end
            cx=cx+1;
        end
        
        % Accumulate signal amplitudes from all the speakers
        amps = amps + amps_speaker;
    end
    
    % Visualize power map at time t
    figure(2);
    log_amps = 20*log10(abs(amps) / 0.00002);
    imagesc(log_amps , [50 100]);
    colormap('parula');
    set(gca, 'XDir','Normal')
    set(gca, 'YDir','Normal')
    set(gca,'FontSize',16)
    colorbar;
    F(ct) = getframe(gcf);
    
    % To calculate root mean square
    evened = evened + amps.^2;
    
    ct = ct + 1;
end

% Calculate RMS
evened = 20*log10(sqrt(evened / (length(trange))) / 0.00002);

if planar_layout <= 0
    output_name = strcat('circular_', num2str(n_speakers), 'speakers_', num2str(height), 'm_', num2str(num_sources), 'sources_', num2str(rotate_angle), '_deg');
else
    output_name = strcat('planar_', num2str(n_speakers), 'speakers_', num2str(height), 'm_', num2str(num_sources), 'sources_', num2str(rotate_angle), '_deg');
end

% Save final results
save(strcat(output_name, '.mat'), 'evened');

% Visualize power map at each time step as a video
writerObj = VideoWriter(strcat(output_name, '_t.mp4'), 'MPEG-4');
writerObj.FrameRate = 5;
open(writerObj);
for i=1:length(F)
    frame = F(i);
    writeVideo(writerObj, frame);
end
close(writerObj);

% Visualize final power map
figure(3);

imagesc(evened, [50 100]);
colormap('parula');
set(gca, 'XDir','Normal')
set(gca, 'YDir','Normal')
set(gca,'FontSize',16)
colorbar;
labels = {'0';'0.25';'0.5';'0.75';'1'};
labeltics = [0.01; 0.25; 0.5; 0.75; 1] * 1/xy_res;
set(gca,'xtick',labeltics,'xticklabel',labels);
set(gca,'ytick',labeltics,'yticklabel',labels);
c = colorbar;
c.Label.String='Sound Pressure Level (dBA)';
xlabel('X (m)');
ylabel('Y (m)');
pbaspect([1 1 1]);

f = getframe(gcf);
imwrite(f.cdata, strcat(output_name, '.png'));


% get amplitude of signal sending from speaker s at time t, distance=dis
% from the speaker
function amp = get_amp(dis, s, t, C, sig, fs)
    delay = dis / C;
    if delay >= t
        amp = 0.;
    else
        amp = sig(s, round((t-delay)*fs));
    end
end