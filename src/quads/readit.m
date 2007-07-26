function [P,back,dist,fieldxy,x0,y0] = readit(fn)
  f = fopen(fn, 'r');
  if (f == -1),
    error('readit', 'failed to open file.\n');
  end
  x1 = fread(f, 1, 'float');
  x2 = fread(f, 1, 'float');
  y1 = fread(f, 1, 'float');
  y2 = fread(f, 1, 'float');
  NF = fread(f, 1, 'float')
  NI = fread(f, 1, 'float')
  fieldarea = fread(f, 1, 'float');
  logprob_background = fread(f, 1, 'float');
  logprob_distractor = fread(f, 1, 'float');
  P = fread(f, [x2-x1+1, y2-y1+1], 'float');
  fieldxy = fread(f, [2,NF], 'float');
  
  back = logprob_background;
  dist = logprob_distractor;
  x0 = x1;
  y0 = y1;
  