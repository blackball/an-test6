import unittest
from django.test.client import Client
from django.test import TestCase

from django.contrib.auth.models import User
from an.portal import views

class LoginTestCases(TestCase):
    # assertContains(response, text, count=None, status_code=200)
    # assertFormError(response, form, field, errors)
    # assertRedirects(response, expected_url, status_code=302, target_status_code=200)
    # assertTemplateUsed(response, template_name)
    
    def setUp(self):
        pass
    
    def login(self):
        self.client.login(username='fred', password='secret')

    def testNotLoggedIn(self):
        pass

    def testBadPassword(self):
        pass

    def testNoSuchUser(self):
        pass

    def testEmptyUsername(self):
        pass

    def testNoAccessAfterLogout(self):
        pass
    
