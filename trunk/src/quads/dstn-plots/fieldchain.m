addpath('.');
out;

N=size(field_radec,1); Nidx=length(field_indexed);
Nquad=size(quads,1); Nconflict=length(conflicts);
iidx=field_indexed+1; quads=quads+1; conflicts=conflicts+1;

indx=setdiff(1:N,iidx);
iquad=unique(sort(quads(:))); Nused=length(iquad);
inquad=setdiff(iidx,iquad);
matchquads=find(codepassed); Nmatch=length(matchquads);
cutquads=setdiff(1:Nquad,matchquads);
imatch0=quads(matchquads,:);
imatch=unique(sort(imatch0(:))); Nobjmatch=length(imatch);
iquadnomatch=setdiff(iquad,imatch); Nobjnomatch=length(iquadnomatch);
Ndropin=size(infield_radec,1);

fsym='rx'; isym='go'; dropinsym='mo';
fontsize=20;
msz0=5; msz1=20;

fra=field_radec(:,1);    fdec=field_radec(:,2);
%ira=field_radec(iidx,1); idec=field_radec(iidx,2);
ira=indexed_radec(:,1); idec=indexed_radec(:,2);

figure(1); clf;


% RAW FIELDS

hhfiqm=plot(fra(imatch),fdec(imatch),fsym); 
hold on;  axis equal; 
hhfiqnm=plot(fra(iquadnomatch),fdec(iquadnomatch),fsym); 
hhfinq=plot(fra(inquad),fdec(inquad),fsym); hold on;  axis equal; 
hhfn=plot(fra(indx),fdec(indx),fsym); 
set(hhfiqm,'MarkerSize',msz0); set(hhfiqnm,'MarkerSize',msz0); 
set(hhfinq,'MarkerSize',msz0); set(hhfn,'MarkerSize',msz0);
xlabel('RA','FontSize',fontsize); ylabel('DEC','FontSize',fontsize);
title('Raw Field Objects','FontSize',fontsize);
mainalims=axis; mainax=gca;
txttop=mainalims(4)-.05*(mainalims(4)-mainalims(3)); 
txtleft=mainalims(1)+.05*(mainalims(2)-mainalims(1)); 
hhtxt=text(txtleft,txttop,sprintf('%d objects in field',N));
drawnow; pause; 


% ADD INDEX STARS

hhiq=plot(ira,idec,isym);
hhiq2=plot(infield_radec(:,1),infield_radec(:,2),isym);
hhdropin=plot(infield_radec(:,1),infield_radec(:,2),...
				  dropinsym,'MarkerSize',msz1);
hhconflict=plot(fra(conflicts),fdec(conflicts),'ws','MarkerSize',msz1);
set(hhfiqm,'MarkerSize',msz1); set(hhfiqnm,'MarkerSize',msz1); 
set(hhfinq,'MarkerSize',msz1);
title('Field Objs. + Index Objs.','FontSize',fontsize);
set(hhtxt,'String',sprintf('%d objects, %d in index, %d in both',...
									N,Nidx+Ndropin-Nconflict,Nidx));
drawnow; pause; 


% ADD QUADS

delete(hhconflict);
set(hhfinq,'MarkerSize',msz0);
delete(hhdropin);
set(hhfiqm,'Color','y'); set(hhfiqnm,'Color','y');
hhquads={};
for nn=1:Nquad
  hhquads{nn}=plotquad(quads(nn,:),fra,fdec);
end
title('Field Objs. + Index Objs. + Index Quads','FontSize',fontsize);
set(hhtxt,'String',...
  sprintf('%d index objects, %d of which make %d quads',Nidx,Nused,Nquad));
drawnow; pause; 

% SHOW CODEDISTS

codeax=axes('position',[.15,.15,.2,.12]);
axes(codeax);
hist(codedist,Nquad); hold on;
alim=axis;
line([codetol;codetol],[alim(3);alim(4)]);
title('Code Tolerances');
axis off;
axes(mainax);
set(hhtxt,'String',...
  sprintf('%d quads, %d codes match',Nquad,Nmatch));
drawnow; pause; 


% REMOVE THOSE THAT DON'T MAKE CODE TOL
for nn=1:length(cutquads) delete(hhquads{cutquads(nn)}); end
delete(codeax);
if(Nobjnomatch)
  set(hhtxt,'String', sprintf('%d matches, using %d objects (%d excluded)'...
									 ,Nmatch,Nobjmatch,Nobjnomatch));
else
  set(hhtxt,'String', sprintf('%d matches, using all %d objects'...
									 ,Nmatch,Nobjmatch));
end
set(hhfiqnm,'MarkerSize',msz0);
hhexcluded=plot(fra(iquadnomatch),fdec(iquadnomatch),'ws','MarkerSize',msz1);
title('Field Objs. + Index Objs. + Matching Quads','FontSize',fontsize);
drawnow; pause; 



% SHOW AGREEMENT

delete(hhexcluded);

