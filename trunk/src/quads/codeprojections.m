figure(1);
mx=max(max(max(max(hist_1_0,hist_2_0),max(max(hist_2_1,hist_3_0),max(hist_3_1,hist_3_2)))));
mn=min(min(min(min(hist_1_0,hist_2_0),min(min(hist_2_1,hist_3_0),min(hist_3_1,hist_3_2)))));
for i=1:3,
  for j=0:(i-1),
    subplot(3,3,(i-1)*3+j+1);
    h=eval(sprintf('hist_%i_%i', i, j));
    s=surf(h);
    set(s, 'EdgeColor', 'interp');
    set(s, 'FaceColor', 'interp');
    %set(s, 'EdgeAlpha', 0);
    set(gca,'XTickLabel',{});
    set(gca,'YTickLabel',{});
    a=axis;
    a(5)=0;
    a(6)=mx*1.1;
    axis(a);
  end
end
colormap hot;

input('next');

if (exist('dhist_xy')),
  mx=max(max(max(max(dhist_1_0,dhist_2_0),max(max(dhist_2_1,dhist_3_0),max(dhist_3_1,dhist_3_2)))));
  mn=min(min(min(min(dhist_1_0,dhist_2_0),min(min(dhist_2_1,dhist_3_0),min(dhist_3_1,dhist_3_2)))));
  for i=1:3,
    for j=0:(i-1),
      subplot(3,3,(i-1)*3+j+1);
      h=eval(sprintf('dhist_%i_%i', i, j));
      s=surf(h);
      set(s, 'EdgeColor', 'interp');
      set(s, 'FaceColor', 'interp');
      %set(s, 'EdgeAlpha', 0);
      set(gca,'XTickLabel',{});
      set(gca,'YTickLabel',{});
      a=axis;
      a(5)=0;
      a(6)=mx*1.1;
      axis(a);
    end
  end
  colormap hot;
  input('next');
end

%figure(2);
for i=1:3,
  for j=0:(i-1),
    subplot(3,3,(i-1)*3+j+1);
    h=eval(sprintf('hist_%i_%i', i, j));
    %s=surf(h);
    %set(s, 'EdgeAlpha', 0);
    %view(0,90);
    imagesc(h);
    axis tight;
    axis square;
    set(gca,'YDir','normal');
    set(gca,'XTickLabel',{});
    set(gca,'YTickLabel',{});
  end
end
colormap hot;

input('next');

if (exist('dhist_xy')),
  for i=1:3,
    for j=0:(i-1),
      subplot(3,3,(i-1)*3+j+1);
      h=eval(sprintf('dhist_%i_%i', i, j));
      imagesc(h);
      axis tight;
      axis square;
      set(gca,'YDir','normal');
      set(gca,'XTickLabel',{});
      set(gca,'YTickLabel',{});
    end
  end
  colormap hot;
  input('next');
end

%figure(3);
mx=max([hist_0 hist_1 hist_2 hist_3]);
for i=1:4,
  subplot(4,1,i);
  h=eval(sprintf('hist_%i', (i-1)));
  b=bar(h, 1);
  axis([0 100 0 1.2*mx]);
  set(gca,'XTickLabel',{});
end

input('next');

if (exist('dhist_xy')),
  mx=max([dhist_0 dhist_1 dhist_2 dhist_3]);
  for i=1:4,
    subplot(4,1,i);
    h=eval(sprintf('dhist_%i', (i-1)));
    b=bar(h, 1);
    axis([0 100 0 1.2*mx]);
    set(gca,'XTickLabel',{});
  end
  input('next');
end

%figure(4);
subplot(1,1,1);
%s=surf(hist_xy);
%set(s, 'EdgeAlpha', 0);
%view(0,90);
imagesc(hist_xy);
set(gca,'YDir','normal');
set(gca,'XTickLabel',{});
set(gca,'YTickLabel',{});
axis tight;
axis square;
xlabel('x')
ylabel('y') 
title('cx vs cy + dx vs dy')
colormap hot

input('next');

if (exist('dhist_xy')),
  subplot(1,1,1);
  imagesc(dhist_xy);
  set(gca,'YDir','normal');
  set(gca,'XTickLabel',{});
  set(gca,'YTickLabel',{});
  axis tight;
  axis square;
  xlabel('x')
  ylabel('y') 
  title('cx vs cy + dx vs dy')
  colormap hot
end
