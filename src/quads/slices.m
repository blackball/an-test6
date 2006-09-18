for x=1:4,
  for y=1:4,
    subplot(4,4,(x-1)*4+y);
    if (x == y),
      hist(codes(:,x), 40);
    else
      plot(codes(:,x), codes(:,y), 'b.', 'MarkerSize', 1);
    end
    set(gca, 'XTickLabel', {});
    set(gca, 'YTickLabel', {});
    axis square;
  end
end

