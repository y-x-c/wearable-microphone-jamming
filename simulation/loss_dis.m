function y = loss_dis(dis, C)
    lambda = C / 25000;
    if dis < 2*lambda
        dis = 2*lambda;
    end
    y = 1/dis;
end