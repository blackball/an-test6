function [hnorm, hdash] = wrapline(x, y)
  dx = x(2:length(x))-x(1:length(x)-1);
  switchinds = find(abs(dx)>=pi);
  if length(switchinds)==0,
    hnorm = line(x,y);
	hdash = [];
    return;
  end;
  dir=sign(dx(switchinds(1)));

  nx=[x(1)];
  for i=1:length(dx),
    nx(i+1)=nx(i)+dx(i);
    if abs(dx(i))>pi,
	  nx(i+1) = nx(i+1) - 2*pi*sign(dx(i));
	end
  end

  %nx
  normx=[];
  normy=[];
  dashx=[];
  dashy=[];

  normx2=[];
  normy2=[];
  dashx2=[];
  dashy2=[];

  %fudge = 0.001;
  fudge = 0;
  inbounds = bitand(nx>=(0-fudge), nx<=(2*pi+fudge));
  inbounds2 = bitand(nx+dir*2*pi>=(0-fudge), nx+dir*2*pi<=(2*pi+fudge));

  inbounds
  inbounds2
  switchinds

  good1=(inbounds(1) && inbounds(2));
  good2=(inbounds2(1) && inbounds2(2));

  for i=2:length(nx),
    if inbounds(i-1) && inbounds(i),
	  normx=[normx, nx(i-1), nx(i)];
	  normy=[normy, y(i-1), y(i)];
	else,
	  dashx=[dashx, nx(i-1), nx(i)];
	  dashy=[dashy, y(i-1),  y(i)];
	end
    if inbounds2(i-1) && inbounds2(i),
	  normx2=[normx2, nx(i-1), nx(i)];
	  normy2=[normy2, y(i-1), y(i)];
	else,
	  dashx2=[dashx2, nx(i-1), nx(i)];
	  dashy2=[dashy2, y(i-1),  y(i)];
	end
  end

  NX=reshape(normx, [2,length(normx)/2]);
  NY=reshape(normy, [2,length(normy)/2]);
  DX=reshape(dashx, [2,length(dashx)/2]);
  DY=reshape(dashy, [2,length(dashy)/2]);
  NX2=reshape(normx2, [2,length(normx2)/2]);
  NY2=reshape(normy2, [2,length(normy2)/2]);
  DX2=reshape(dashx2, [2,length(dashx2)/2]);
  DY2=reshape(dashy2, [2,length(dashy2)/2]);

  hnorm = line([NX, NX2], [NY, NY2]);
  hdash = line([DX, DX2], [DY, DY2]);

  if 0,
  sw=1;
  good=1;
  for i=2:length(nx),
	if good,
	  normx=[normx, nx(i-1), nx(i)];
	  normy=[normy, y(i-1), y(i)];
	else,
	  dashx=[dashx, nx(i-1), nx(i)];
	  dashy=[dashy, y(i-1),  y(i)];
	end
    if i == switchinds(sw),
	  good = ~good;
	  sw = sw+1;
	  if sw > length(switchinds),
	    sw = 1;
	  end
	end
  end
  end

  if 0,
    NX=reshape(normx, [2,length(normx)/2]);
	NY=reshape(normy, [2,length(normy)/2]);
	DX=reshape(dashx, [2,length(dashx)/2]);
	DY=reshape(dashy, [2,length(dashy)/2]);

    hnorm = line([NX, DX + dir*2*pi], [NY, DY]);
    hdash = line([DX, NX + dir*2*pi], [DY, NY]);
  end
  set(hdash, 'LineStyle', '--');

return;

