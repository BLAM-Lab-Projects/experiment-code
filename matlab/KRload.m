
function [data, header, filename, filepath] = KRload(filename, filepath)
% KRLOAD Load in kinereach data (new c++ code), 
% parsing the header file and the raw data.
%
%     [data, header, filename, filepath] = KRLOAD(filename)
%     The filename is the name of the file, e.g. 'mydata.dat', and
%     the filepath is the full path to the data, e.g. 'C:/Users/data/'.
% 
%     If the filename and path is unspecified, a UI will open to allow
%     manual selection of the file. If just the path is unspecified, the
%     current working directory is assumed (and a warning emitted).
%    

    if nargin == 0
        [filename, filepath] = uigetfile('*.*', 'Select data file to analyze.');
    elseif ~exist('filename')
        error('Must provide a filename.');
    elseif ~exist('filepath')
        warning('Assuming the current path is correct, may fail.');
        filepath = pwd;
    end

    fid = fopen([filepath filename], 'r');

    while true
        temp = fgetl(fid);
        if strcmp(temp, '--')
            %detect the end of the header
            temp = fgetl(fid);
            header.cols = textscan(temp, '%s');
            header.cols = header.cols{1};
            fgetl(fid);
            break
        else
            hvals = textscan(temp, '%s %s');
            if ~isempty(str2num(char(hvals{2})))
                hvals = textscan(temp, '%s %d');
                tmp = strrep(char(hvals{1}), ':', '');
                eval(sprintf('header.%s = %d;', tmp, hvals{2}));
            else
                tmp = strrep(char(hvals{1}), ':', '');
                tmp1 = strrep(char(hvals{2}), '.txt', '');
                eval(sprintf('header.%s = ''%s'';', tmp, char(tmp1)));
            end          
        end
    end

    % unknown columns, use this code for complete flexiblity
    readstr = '';
    for a = 1:length(header.cols)
        switch(char(header.cols{a}))
            case {'Device_Num', 'FakeTime', 'Time'}
                readstr = [readstr '%d '];
            case {'HandX', 'HandY', 'HandZ', 'HandYaw', 'HandPitch', 'HandRoll', 'Theta'}
                readstr = [readstr '%f '];
            case {'StartX', 'StartY', 'TargetX', 'TargetY'}
                readstr = [readstr '%f '];
            case 'Trace'
                readstr = [readstr '%d '];
            case 'Keymap'
                readstr = [readstr '%s '];
            otherwise
                readstr = [readstr '%f '];
        end
    end

    dvals = textscan(fid, readstr, inf);

    %compute number of birds
    numBirds = length(unique(dvals{1}));

    for a = 1:numBirds

        %divide data structure into appropriate fields, based on column names
        for b = 1:length(header.cols)
            eval(sprintf('data{%d}.%s = dvals{%d}(%d:%d:end);', a, char(header.cols{b}), b, a, numBirds));
        end

        if isfield(header, 'Sampling_Rate') && isfield(data{a},'HandX')
            data{a}.MakeTime = [0:1:length(data{a}.HandX)-1]'/header.Sampling_Rate*1000;
        elseif isfield(data{a}, 'HandX')
            data{a}.MakeTime = [0:1:length(data{a}.HandX) - 1]';
        elseif isfield(header, 'Sampling_Rate') && isfield(data{a},'StickX')
            data{a}.MakeTime = [0:1:length(data{a}.StickX) - 1]'/header.Sampling_Rate*1000;
        elseif isfield(data{a}, 'StickX')
            data{a}.MakeTime = data{a}.SystemTime - data{a}.SystemTime(1);
        else  %unrecognized trial table type
            data{a}.MakeTime = [];
        end

        %identify device, if possible
        if isfield(data{a},'Device_Num')
            data{a}.Device_Num = unique(data{a}.Device_Num);
            switch data{a}.Device_Num(1)
                case 1
                    data{a}.BirdName = 'Left_Hand';
                case 2
                    data{a}.BirdName = 'Left_Arm';
                case 3
                    data{a}.BirdName = 'Right_Hand';
                case 4
                    data{a}.BirdName = 'Right_Arm';
            end %end switch 
        end %end if

    end %end for loop

    fclose(fid);
end
