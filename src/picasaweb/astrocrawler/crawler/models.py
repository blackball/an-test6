from django.db import models

MAX_URL_LENGTH = 400

class UserURL(models.Model):
    url = models.CharField(maxlength=MAX_URL_LENGTH)

class Image(models.Model):
    md5sum = models.CharField(maxlength=32)
    local_copy = models.CharField(maxlength=400)
    first_downloaded = models.DateTimeField()
    first_downloaded_from = models.ForeignKey(UserURL)

class GalleryEntry(models.Model):
    image = models.ForeignKey(Image)
    gallery_url = models.CharField(maxlength=MAX_URL_LENGTH)
    added = models.DateTimeField()

class GalleryEntryToUserURLs(models.Model):
    entry = models.ForeignKey(GalleryEntry)
    url = models.ForeignKey(UserURL)

class TagEvent(models.Model):
    seen = models.DateTimeField()
    downloaded = models.DateTimeField()
    user_url = models.ForeignKey(UserURL)
    solved_attempted = models.DateTimeField()
    solved_completed = models.DateTimeField()
    image = models.ForeignKey(Image)
    entry = models.ForeignKey(GalleryEntry)
    solve_tag = models.ForeignKey('self') # Refers internally

class GalleryEntryToTagEvents(models.Model):
    entry = models.ForeignKey(GalleryEntry)
    tag = models.ForeignKey(TagEvent)

class Solve(models.Model):
    image = models.ForeignKey(Image)
    tag = models.ForeignKey(TagEvent)
    entry = models.ForeignKey(GalleryEntry)
    # TODO - add other stuff.
