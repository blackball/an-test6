#!/usr/bin/python2.4
# Copyright 2010 Keir Mierle.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
__author__ = 'mierle@gmail.com (Keir Mierle)'


import web
import json
import time


exposed = {}


def get(*args, **kwargs):
  default_args = kwargs
  def real_expose(f):
    path = '/' + f.__name__
    def JsonF(*args, **kwargs):
      return json.write(f(*args, **kwargs))
    class Handler:
      def GET(self):
        args = web.input(**default_args)
        start = time.time()
        result = JsonF(args)
        elapsed = time.time() - start
        if elapsed > 0.5:
          print path, 'took', elapsed, 'seconds'
        return result
    exposed[path] = Handler
    return f

  if len(args) == 1 and len(kwargs) == 0:
    # Decorator was used as @get
    return real_expose(args[0])
  # Decorator was used as @get(arg1=val, arg2=val, ...)
  return real_expose


def post(*args, **kwargs):
  default_args = kwargs
  def real_expose(f):
    def JsonF(*args, **kwargs):
      return json.write(f(*args, **kwargs))
    class Handler:
      def POST(self):
        args = web.input(**default_args)
        return JsonF(args)
    exposed['/' + f.__name__] = Handler
    return f
  if len(args) == 1 and len(kwargs) == 0:
    return real_expose(args[0])
  return real_expose


def app():
  t = sum(exposed.items(), ())
  if '/index' in exposed:
    t = t + ('/.*', exposed['/index'])
  return web.application(t, exposed)
#!/usr/bin/python2.4
# Copyright 2010 Keir Mierle.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
__author__ = 'mierle@gmail.com (Keir Mierle)'


import web
import json
import time


exposed = {}


def get(*args, **kwargs):
  default_args = kwargs
  def real_expose(f):
    path = '/' + f.__name__
    def JsonF(*args, **kwargs):
      return json.write(f(*args, **kwargs))
    class Handler:
      def GET(self):
        args = web.input(**default_args)
        start = time.time()
        result = JsonF(args)
        elapsed = time.time() - start
        if elapsed > 0.5:
          print path, 'took', elapsed, 'seconds'
        return result
    exposed[path] = Handler
    return f

  if len(args) == 1 and len(kwargs) == 0:
    # Decorator was used as @get
    return real_expose(args[0])
  # Decorator was used as @get(arg1=val, arg2=val, ...)
  return real_expose


def post(*args, **kwargs):
  default_args = kwargs
  def real_expose(f):
    def JsonF(*args, **kwargs):
      return json.write(f(*args, **kwargs))
    class Handler:
      def POST(self):
        args = web.input(**default_args)
        return JsonF(args)
    exposed['/' + f.__name__] = Handler
    return f
  if len(args) == 1 and len(kwargs) == 0:
    return real_expose(args[0])
  return real_expose


def app():
  t = sum(exposed.items(), ())
  if '/index' in exposed:
    t = t + ('/.*', exposed['/index'])
  return web.application(t, exposed)
