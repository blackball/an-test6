function [hnorm, hdash] = wrapline(x, y)
  dx = x(2:length(x))-x(1:length(x)-1);
  switchinds = find(abs(dx)>=pi);
  if length(switchinds)==0,
    hnorm = line(x,y);
	hdash = [];
    return;
  end;
  dir=sign(dx(switchinds(1)))

  nx=[x(1)];
  for i=1:length(dx),
    nx(i+1)=nx(i)+dx(i);
    if abs(dx(i))>pi,
	  nx(i+1) = nx(i+1) - 2*pi*sign(dx(i));
	end
  end

  nx
  normx=[];
  normy=[];
  dashx=[];
  dashy=[];
  inbounds = bitand(nx>=0, nx<=2*pi)
  for i=2:length(nx),
    if inbounds(i) && inbounds(i-1),
	  normx = [normx, nx(i-1), nx(i)];
	  normy = [normy, y(i-1),  y(i) ];
	else,
	  dashx = [dashx, nx(i-1), nx(i)];
	  dashy = [dashy, y(i-1),  y(i) ];
	end
  end

  NX=reshape(normx, [2,length(normx)/2]);
  NY=reshape(normy, [2,length(normy)/2]);
  DX=reshape(dashx, [2,length(dashx)/2]);
  DY=reshape(dashy, [2,length(dashy)/2]);

  hnorm = line([NX, DX + dir*2*pi], [NY, DY]);
  hdash = line([DX, NX + dir*2*pi], [DY, NY]);
  set(hdash, 'LineStyle', '--');

return;

