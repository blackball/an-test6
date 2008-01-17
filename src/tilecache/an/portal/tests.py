from unittest import TestLoader

from an.portal.test_login import LoginTestCases

# Granting the right to create databases:
# as root: pgsql postgres
#  > \du   # to list users
#  > ALTER ROLE gmaps CREATEDB;

def suite():
    login_suite = TestLoader().loadTestsFromTestCase(LoginTestCases)
    return login_suite
