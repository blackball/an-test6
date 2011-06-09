from settings_common import *

DATABASES['default']['NAME'] = 'an-supernova'

LOGGING['loggers']['django.request']['level'] = 'WARN'

SESSION_COOKIE_NAME = 'SupernovaAstrometrySession'

ssh_solver_config = 'an-supernova'
