sum_hist_0 = 0;
sum_hist_1 = 0;
sum_hist_2 = 0;
sum_hist_3 = 0;
sum_dhist_0 = 0;
sum_dhist_1 = 0;
sum_dhist_2 = 0;
sum_dhist_3 = 0;
sum_hist_1_0 = 0;
sum_hist_2_0 = 0;
sum_hist_2_1 = 0;
sum_hist_3_0 = 0;
sum_hist_3_1 = 0;
sum_hist_3_2 = 0;
sum_dhist_1_0 = 0;
sum_dhist_2_0 = 0;
sum_dhist_2_1 = 0;
sum_dhist_3_0 = 0;
sum_dhist_3_1 = 0;
sum_dhist_3_2 = 0;
sum_hist_xy = 0;
sum_dhist_xy = 0;

for j=0:11,
  eval(sprintf('proj%02i', j));

  sum_hist_0 = sum_hist_0 + hist_0;
  sum_hist_1 = sum_hist_1 + hist_1;
  sum_hist_2 = sum_hist_2 + hist_2;
  sum_hist_3 = sum_hist_3 + hist_3;
  sum_dhist_0 = sum_dhist_0 + dhist_0;
  sum_dhist_1 = sum_dhist_1 + dhist_1;
  sum_dhist_2 = sum_dhist_2 + dhist_2;
  sum_dhist_3 = sum_dhist_3 + dhist_3;
  sum_hist_1_0 = sum_hist_1_0 + hist_1_0;
  sum_hist_2_0 = sum_hist_2_0 + hist_2_0;
  sum_hist_2_1 = sum_hist_2_1 + hist_2_1;
  sum_hist_3_0 = sum_hist_3_0 + hist_3_0;
  sum_hist_3_1 = sum_hist_3_1 + hist_3_1;
  sum_hist_3_2 = sum_hist_3_2 + hist_3_2;
  sum_dhist_1_0 = sum_dhist_1_0 + dhist_1_0;
  sum_dhist_2_0 = sum_dhist_2_0 + dhist_2_0;
  sum_dhist_2_1 = sum_dhist_2_1 + dhist_2_1;
  sum_dhist_3_0 = sum_dhist_3_0 + dhist_3_0;
  sum_dhist_3_1 = sum_dhist_3_1 + dhist_3_1;
  sum_dhist_3_2 = sum_dhist_3_2 + dhist_3_2;
  sum_hist_xy = sum_hist_xy + hist_xy;
  sum_dhist_xy = sum_dhist_xy + dhist_xy;

end

hist_0 = sum_hist_0;
hist_1 = sum_hist_1;
hist_2 = sum_hist_2;
hist_3 = sum_hist_3;
dhist_0 = sum_dhist_0;
dhist_1 = sum_dhist_1;
dhist_2 = sum_dhist_2;
dhist_3 = sum_dhist_3;
hist_1_0 = sum_hist_1_0;
hist_2_0 = sum_hist_2_0;
hist_2_1 = sum_hist_2_1;
hist_3_0 = sum_hist_3_0;
hist_3_1 = sum_hist_3_1;
hist_3_2 = sum_hist_3_2;
dhist_1_0 = sum_dhist_1_0;
dhist_2_0 = sum_dhist_2_0;
dhist_2_1 = sum_dhist_2_1;
dhist_3_0 = sum_dhist_3_0;
dhist_3_1 = sum_dhist_3_1;
dhist_3_2 = sum_dhist_3_2;
hist_xy = sum_hist_xy;
dhist_xy = sum_dhist_xy;


