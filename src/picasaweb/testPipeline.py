from pwlib import *
pws = pwInit()
pwAuth(pws)

print pws.email

a=insertAlbumNonDuplicate("astrometrynet","testalbum",pws,verbose=True)
