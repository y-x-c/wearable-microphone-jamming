function y = loss_dir(ang)
    if ang == 0
        y = 1;
        return;
    end

    freq = 25000;
    r_transducer = 0.0164/2;
    k = freq * 2 * pi / (340);
    ang2 = ang;
    ang2(abs(ang) > pi/2) = pi/2;
    x = k * sin(ang2) * r_transducer;
    J1 = besselj(1, x);
    y = 2 * J1 ./ x;
end