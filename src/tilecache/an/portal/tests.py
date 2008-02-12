from unittest import TestLoader, TestSuite

from an.portal.test_login import LoginTestCases
from an.portal.test_newjob import NewJobTestCases

def suite():
    all_suites = TestSuite()

    for x in [ LoginTestCases, NewJobTestCases ]:
        suite  = TestLoader().loadTestsFromTestCase(x)
        all_suites.addTest(suite)

    #suite  = TestLoader().loadTestsFromTestCase(LoginTestCases)
    #all_suites.addTest(suite)

    #suite = TestLoader().loadTestsFromTestCase(NewJobTestCases)
    #all_suites.addTest(suite)

    return all_suites
