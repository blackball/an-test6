from django.db import backend
from django.utils.datastructures import SortedDict
from django.db import models
import logging

def handle_legacy_orderlist(order_list):
	return models.query.handle_legacy_orderlist(order_list)

class MyQuerySet(models.query.QuerySet):
	_orderby_sql = None

	def order_by_expression(self, sql):
		return self._clone(_orderby_sql = sql)

	def OLD_get_sql_clause(self):
		logging.debug("my _get_sql_clause called.")
		if self._orderby_sql:
			logging.debug("	 orderby_sql = " + self._orderby_sql)
		else:
			logging.debug("	 orderby_sql = None")
		select, sql, params = super(MyQuerySet, self)._get_sql_clause()
		if self._orderby_sql:
			sql += ' ORDER BY ' + self._orderby_sql
		logging.debug("Returning SQL: " + sql)
		return select, sql, params

	def _super_get_sql_orderby_clause(self, opts):
		order_by = []
		if self._order_by is not None:
			ordering_to_use = self._order_by
		else:
			ordering_to_use = opts.ordering
		for f in handle_legacy_orderlist(ordering_to_use):
			if f == '?': # Special case.
				order_by.append(backend.get_random_function_sql())
			else:
				if f.startswith('-'):
					col_name = f[1:]
					order = "DESC"
				else:
					col_name = f
					order = "ASC"
				if "." in col_name:
					table_prefix, col_name = col_name.split('.', 1)
					table_prefix = backend.quote_name(table_prefix) + '.'
				else:
					# Use the database table as a column prefix if it wasn't given,
					# and if the requested column isn't a custom SELECT.
					if "." not in col_name and col_name not in (self._select or ()):
						table_prefix = backend.quote_name(opts.db_table) + '.'
					else:
						table_prefix = ''
					order_by.append('%s%s %s' % (table_prefix, backend.quote_name(orderfield2column(col_name, opts)), order))
		if order_by:
			return "ORDER BY " + ", ".join(order_by)
		return None

	def _get_sql_orderby_clause(self, opts):
		if self._orderby_sql:
			return self._orderby_sql
		return self._super_get_sql_orderby_clause(opts)

	def _get_sql_clause(self):
		### This whole block was copy-n-pasted from Django's query.py because it wasn't written in a way
		### that allows it to be extended...

		opts = self.model._meta

		# Construct the fundamental parts of the query: SELECT X FROM Y WHERE Z.
		select = ["%s.%s" % (backend.quote_name(opts.db_table), backend.quote_name(f.column)) for f in opts.fields]
		tables = [quote_only_if_word(t) for t in self._tables]
		joins = SortedDict()
		where = self._where[:]
		params = self._params[:]

		# Convert self._filters into SQL.
		joins2, where2, params2 = self._filters.get_sql(opts)
		joins.update(joins2)
		where.extend(where2)
		params.extend(params2)

		# Add additional tables and WHERE clauses based on select_related.
		if self._select_related:
			fill_table_cache(opts, select, tables, where, 
							 old_prefix=opts.db_table, 
							 cache_tables_seen=[opts.db_table], 
							 max_depth=self._max_related_depth)

		# Add any additional SELECTs.
		if self._select:
			select.extend(['(%s) AS %s' % (quote_only_if_word(s[1]), backend.quote_name(s[0])) for s in self._select.items()])

		# Start composing the body of the SQL statement.
		sql = [" FROM", backend.quote_name(opts.db_table)]

		# Compose the join dictionary into SQL describing the joins.
		if joins:
			sql.append(" ".join(["%s %s AS %s ON %s" % (join_type, table, alias, condition)
								 for (alias, (table, join_type, condition)) in joins.items()]))

		# Compose the tables clause into SQL.
		if tables:
			sql.append(", " + ", ".join(tables))

		# Compose the where clause into SQL.
		if where:
			sql.append(where and "WHERE " + " AND ".join(where))

		# ORDER BY clause
		orderby_clause = self._get_sql_orderby_clause(opts)
		if orderby_clause:
			sql.append(orderby_clause)

		# LIMIT and OFFSET clauses
		if self._limit is not None:
			sql.append("%s " % backend.get_limit_offset_sql(self._limit, self._offset))
		else:
			assert self._offset is None, "'offset' is not allowed without 'limit'"

		logging.debug("Returning SQL: " + " ".join(sql))

		return select, " ".join(sql), params

	def __str__(self):
		return '<MyQuerySet: _orderby_sql=' + (self._orderby_sql or "none") + '>'

	def _clone(self, klass=None, **kwargs):
		logging.debug("_clone: self = " + str(self))
		c = super(MyQuerySet, self)._clone(klass, **kwargs)
		c._orderby_sql = self._orderby_sql
		#c.__dict__.update(kwargs)
		if '_orderby_sql' in kwargs:
			c._orderby_sql = kwargs['_orderby_sql']
		logging.debug("_clone: c = " + str(c))
		return c
		#logging.debug("_clone: super is " + str(type(super(MyQuerySet, self))))
		#logging.debug("_clone: super2 is " + str(type(super(self.__class__, self))))
		#s = super(MyQuerySet, self)
		#logging.debug("_clone: super3 is " + str(type(s)))
		#logging.debug("_clone: super3 is " + str(s))
		#cl = s._clone
		#logging.debug("_clone: _clone is " + str(cl))
		#c = s._clone(klass, kwargs)
		#c = models.query.QuerySet._clone(self, klass, kwargs)
		#c = models.query.QuerySet._clone(klass, kwargs)
		#c = models.query.QuerySet._clone(self, klass, **kwargs)

class MyManager(models.Manager):

	def get_query_set(self):
		return MyQuerySet(self.model)

class Image(models.Model):
	objects = MyManager()
	
	# The original image filename.
	origfilename = models.CharField(maxlength=1024)
	# The original image file format.
	origformat = models.CharField(maxlength=30)
	# The JPEG base filename.
	filename = models.CharField(maxlength=1024)
	ramin =	 models.FloatField(decimal_places=7, max_digits=10)
	ramax =	 models.FloatField(decimal_places=7, max_digits=10)
	decmin = models.FloatField(decimal_places=7, max_digits=10)
	decmax = models.FloatField(decimal_places=7, max_digits=10)
	imagew = models.IntegerField()
	imageh = models.IntegerField()

	def __str__(self):
		return self.origfilename

	# Include it in the Django admin page?
	#class Admin:
	#	pass

