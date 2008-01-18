import unittest
from django.test.client import Client
from django.test import TestCase

from django.core.urlresolvers import reverse

from django.contrib.auth.models import User
from an.portal import views

class LoginTestCases(TestCase):
    # assertContains(response, text, count=None, status_code=200)
    # assertFormError(response, form, field, errors)
    # assertRedirects(response, expected_url, status_code=302, target_status_code=200)
    # assertTemplateUsed(response, template_name)
    
    def setUp(self):
        self.urlprefix = 'http://testserver'
        # create some dummy accounts
        self.u1 = 'test1@astrometry.net'
        self.p1 = 'password1'
        accts = [ (self.u1, self.p1),
                  ('test2@astrometry.net', 'password2'), ]
        for (e, p) in accts:
            User.objects.create_user(e, e, p).save()
        #self.loginurl = reverse('an.portal.views.login')
        self.loginurl = reverse('django.contrib.auth.views.login')

    def login1(self):
        self.client.login(username=self.u1, password=self.p1)

    def testLoginForm(self):
        print 'Login url is %s' % self.loginurl
        resp = self.client.get(self.loginurl)
        self.assertEqual(resp.status_code, 200)
        self.assertTemplateUsed(resp, 'portal/login.html')
        # print 'Got: %s' % resp.content

    def testEmptyUsername(self):
        resp = self.client.post(self.loginurl,
                                { 'username': '', 'password': 'pass' })
        self.assertFormError(resp, 'form', 'username', 'This field is required.')

    def testEmptyPassword(self):
        resp = self.client.post(self.loginurl,
                                { 'username': 'bob', 'password': '' })
        self.assertFormError(resp, 'form', 'password', 'This field is required.')

    def testEmptyBoth(self):
        resp = self.client.post(self.loginurl,
                                { 'username': '', 'password': '' })
        self.assertFormError(resp, 'form', 'username', 'This field is required.')
        self.assertFormError(resp, 'form', 'password', 'This field is required.')

    # FIXME - this should maybe move to test_job_summary:
    def testJobSummaryRedirects(self):
        url = reverse('an.portal.views.summary')
        resp = self.client.get(url)
        redirurl = self.urlprefix + self.loginurl + '?next=' + url
        self.assertRedirects(resp, redirurl)

    # FIXME - add other redirect tests.

    # check that when a user is logged in, they don't get redirected to the
    # login page.
    def testLoggedInNoRedirect(self):
        url = reverse('an.portal.views.summary')
        self.login1()
        resp = self.client.get(url)
        self.assertEqual(resp.status_code, 200)

    def testLoginWorks(self):
        resp = self.client.post(self.loginurl, { 'username': self.u1, 'password': self.p1 })
        url = reverse('an.portal.newjob.newurl')
        self.assertRedirects(resp, self.urlprefix + url)

    def assertBadUsernamePasswordPair(self, resp):
        self.assertFormError(resp, 'form', '__all__',
                             'Please enter a correct username and password. Note that both fields are case-sensitive.')

    def testBadUsername(self):
        resp = self.client.post(self.loginurl,
                                { 'username': 'u', 'password': 'smeg' })
        self.assertBadUsernamePasswordPair(resp)

    def testBadPassword(self):
        resp = self.client.post(self.loginurl,
                                { 'username': self.u1, 'password': 'smeg' })
        self.assertBadUsernamePasswordPair(resp)
        #for c in resp.context:
        #    errs = c['form'].errors
        #    for f,e in errs.items():
        #        print 'Field "%s": error "%s"' % (f, e)

    def testNoSuchUser(self):
        resp = self.client.post(self.loginurl,
                                { 'username': 'nobody@nowhere.com', 'password': 'smeg' })
        self.assertBadUsernamePasswordPair(resp)

    def testNoAccessAfterLogout(self):
        pass
    
