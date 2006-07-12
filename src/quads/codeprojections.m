figure(1);
mx=max(max(max(max(hist_1_0,hist_2_0),max(max(hist_2_1,hist_3_0),max(hist_3_1,hist_3_2)))));
mn=min(min(min(min(hist_1_0,hist_2_0),min(min(hist_2_1,hist_3_0),min(hist_3_1,hist_3_2)))));
for i=1:3,
  for j=0:(i-1),
    subplot(3,3,(i-1)*3+j+1);
    h=eval(sprintf('hist_%i_%i', i, j));
    surf(h);
    set(gca,'XTickLabel',{});
    set(gca,'YTickLabel',{});
    a=axis;
    a(5)=0;
    a(6)=mx*1.2;
    axis(a);
  end
end
colormap hot;

figure(2);
for i=1:3,
  for j=0:(i-1),
    subplot(3,3,(i-1)*3+j+1);
    h=eval(sprintf('hist_%i_%i', i, j));
    s=surf(h);
	set(s, 'EdgeAlpha', 0);
	view(0,90);
	axis tight;
	axis square;
    set(gca,'XTickLabel',{});
    set(gca,'YTickLabel',{});
  end
end
colormap hot;

figure(3);
mx=max([hist_0 hist_1 hist_2 hist_3]);
for i=1:4,
  subplot(4,1,i);
  h=eval(sprintf('hist_%i', (i-1)));
  b=bar(h, 1);
  axis([0 100 0 1.2*mx]);
  set(gca,'XTickLabel',{});
end

