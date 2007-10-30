import logging

from an import gmaps_config


logfile = gmaps_config.portal_logfile
logging.basicConfig(level=logging.DEBUG,
                    #format='%(asctime)s %(levelname)s %(message)s',
                    format='%(message)s',
                    filename=logfile,
                    )

def log(*msg):
    logging.debug(' '.join(msg))
