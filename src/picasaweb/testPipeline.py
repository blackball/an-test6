from pwlib import *
pws = pwInit()
pwAuth(pws,"")

insertAlbumNonDuplicate("astrometrynet","testalbum",pws,verbose=True)
insertAlbumNonDuplicate("astrometrynet","testalbum",pws,verbose=True)
