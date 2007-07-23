function [h] = trunchist(x, lo, step, hi, dolo, dohi)

edges = [lo:step:hi];
if (dolo)
  edges = [-inf, lo-2*step, lo-step, edges];
end
if (dohi)
  edges = [edges, hi+step, inf];
end

y = histc(x, edges);

if (dolo)
  y(2) = y(1) + y(2) + y(3);
  y(3) = 0;
end
if (dohi)
  y(length(y)-1) = y(length(y)-1) + y(length(y)-2);
  y(length(y)-2) = 0;
end

if dolo,
  ilo = 2;
else
  ilo = 1;
end
if dohi,
  ihi = length(y)-1;
else
  ihi = length(y);
end

h = bar(edges(ilo:ihi), y(ilo:ihi), 'histc');
a=axis;
if dohi,
  a(2) = hi + 2*step;
else
  a(2) = hi;
end
if dolo,
  a(1) = lo - 2*step;
else,
  a(1) = lo;
end
axis(a);

xt = get(gca, 'XTick');
xtl = get(gca, 'XTickLabel');

xtl2 = {};
for i=1:length(xtl),
  xtl2{i + dolo} = xtl(i,:);
end

if dohi,
  xt = [xt, hi+2*step];
  xtl2{length(xtl)+1} = '>';
end
if dolo,
  xt = [lo-2*step, xt];
  xtl2{1} = '<';
end

set(gca, 'XTick', xt);
set(gca, 'XTickLabel', xtl2);

