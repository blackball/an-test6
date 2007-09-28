from django.http import HttpResponse
from django.template import Context, loader
from an.tile.models import Image
from django.db.models import Q

def query(request):
	try:
		bb = request.GET['BBOX']
	except (KeyError):
		return HttpResponse('No BBOX')
	bbvals = bb.split(',')
	if (len(bbvals) != 4):
		return HttpResponse('Bad BBOX')
	ramin  = float(bbvals[0])
	decmin = float(bbvals[1])
	ramax  = float(bbvals[2])
	decmax = float(bbvals[3])
	dec_ok = Image.objects.filter(decmin__lte=decmax, decmax__gte=decmin)
	Q_normal = Q(ramin__lte=ramax) & Q(ramax__gte=ramin)
	raminwrap = ramin - 360
	ramaxwrap = ramax - 360
	Q_wrap   = Q(ramin__lte=ramaxwrap) & Q(ramax__gte=raminwrap)
	imgs = dec_ok.filter(Q_normal | Q_wrap)

	res = HttpResponse()
	res['Content-Type'] = 'image/png'
	res.write('Hello World!')
	return res

#t = loader.get_template('index.html')
#c = Context({
#   'all_imgs': imgs,
#   })
#return HttpResponse(t.render(c))
