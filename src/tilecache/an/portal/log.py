import logging

from an import gmaps_config


logfile = gmaps_config.portal_logfile
logging.basicConfig(level=logging.DEBUG,
                    #format='%(asctime)s %(levelname)s %(message)s',
                    format='%(message)s',
                    filename=logfile,
                    )

def log(*msg):
<<<<<<< .mine
    #s = ' '.join([str(m).encode('unicode_escape') for m in msg]
    s = ' '.join([str(m) for m in msg]
    logging.debug(s)
    #logging.debug(' '.join([str(m).encode('latin_1') for m in msg]))
=======
    logging.debug(' '.join([str(m).decode('latin_1', 'backslashreplace') for m in msg]))
    #logging.debug(' '.join([str(m).encode('latin_1', 'backslashreplace') for m in msg]))
    #logging.debug(' '.join([str(m).encode('latin_1') for m in msg]))
>>>>>>> .r7457
    #logging.debug(' '.join(map(str, msg)))
