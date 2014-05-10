/*
 * myredis_mysql.c
 *
 *  Created on: 2014. 5. 9.
 *      Author: powerumc
 */

#include "myredis_mysql.h"

char *myredis_strcat(const char *str, const char *pfix);


#ifndef REDIS_SET_NO_FLAGS
#define REDIS_SET_NO_FLAGS 0
#endif
#ifndef REDIS_SET_NX
#define REDIS_SET_NX (1<<0)     /* Set if key not exists. */
#endif
#ifndef REDIS_SET_XX
#define REDIS_SET_XX (1<<1)     /* Set if key exists. */
#endif

char* itoa(int val, int base) {

	static char buf[32] = {0};
	int i = 30;

	for(; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	return &buf[i+1];
}

MYSQL* conn;
void mysqlqCommand(redisClient *c) {

	MYSQL *mysql = myredis_connect(c);
	if (!mysql) return;

	if (c->argc == 3)
		myredis_query(c, mysql);
	else if (c->argc > 3)
		mysqlqsCommand(c, mysql);

	myredis_disconnect(mysql);
}

char *myredis_strcat(const char *str, const char *pfix) {
	char k[strlen(str) + 1];
	strcpy(k, str);
	char *ptr = strcat(k, pfix);

	return ptr;
}

robj *myredis_lookupKeyRead(redisClient *c, robj *key, const char *pfix) {

	char k[strlen((char *)key->ptr) + 1];
	strcpy(k, (char *)key->ptr);
	char *ptr = strcat(k, pfix);

	robj *key_ = createStringObject(ptr, strlen(ptr));
	robj *res = lookupKeyRead(c->db, key_);

	return res;
}



void mysqlqsCommand(redisClient *c, MYSQL* mysql) {
	robj *expire = NULL;
	int unit = UNIT_SECONDS;
	int flags = REDIS_SET_NO_FLAGS;

	for (int j = 4; j < c->argc; j++) {
		char *a = c->argv[j]->ptr;
		robj *next = (j == c->argc-1) ? NULL : c->argv[j+1];

		if ((a[0] == 'n' || a[0] == 'N') &&
			(a[1] == 'x' || a[1] == 'X') && a[2] == '\0') {
			flags |= REDIS_SET_NX;
		} else if ((a[0] == 'x' || a[0] == 'X') &&
				   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0') {
			flags |= REDIS_SET_XX;
		} else if ((a[0] == 'e' || a[0] == 'E') &&
				   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' && next) {
			unit = UNIT_SECONDS;
			expire = next;
			j++;
		} else if ((a[0] == 'p' || a[0] == 'P') &&
				   (a[1] == 'x' || a[1] == 'X') && a[2] == '\0' && next) {
			unit = UNIT_MILLISECONDS;
			expire = next;
			j++;
		} else {
			addReply(c,shared.syntaxerr);
			return;
		}
	}

	if (!mysql) return;

	robj *res = myredis_query_scalar(c, mysql);
	if (res) {
		c->argv[2] = tryObjectEncoding(c->argv[2]);
		setGenericCommand(c,flags,c->argv[3],res,expire,unit,NULL,NULL);
	}
}

myredis_conn myredis_connect_get(redisClient *c) {

	myredis_conn myredis;
	myredis.host   = myredis_lookupKeyRead(c, c->argv[1], ".host");
	myredis.user   = myredis_lookupKeyRead(c, c->argv[1], ".user");
	myredis.passwd = myredis_lookupKeyRead(c, c->argv[1], ".passwd");
	myredis.db 	   = myredis_lookupKeyRead(c, c->argv[1], ".db");
	myredis.port   = myredis_lookupKeyRead(c, c->argv[1], ".port");

	return myredis;
}

MYSQL *myredis_connect(redisClient *c) {

	myredis_conn conn = myredis_connect_get(c);

	if (!conn.host)   { addReplyError(c, "mysql: host key could not found."); return NULL; }
	if (!conn.user)   { addReplyError(c, "mysql: user key could not found."); return NULL; }
	if (!conn.passwd) { addReplyError(c, "mysql: passwd key could not found."); return NULL; }
	if (!conn.port) {
		char *n = itoa(MYREDIS_MYSQL_PORT, 10);
		conn.port = createStringObject(n, strlen(n));
	}
	if (!conn.db) {
		conn.db = createStringObject("", 0);
	}

	redisLog(REDIS_DEBUG, "mysql: init.");
	MYSQL *mysql = mysql_init(NULL);
	if (!mysql) {
		addReplyError(c, "mysql init failed.");
		return NULL;
	}
	redisLog(REDIS_DEBUG, "mysql: init success.");


	redisLog(REDIS_DEBUG, "mysql: connecting.");
	MYSQL *res = mysql_real_connect(mysql, conn.host->ptr,
			                               conn.user->ptr,
	                                       conn.passwd->ptr,
	                                       conn.db->ptr,
	                                       atoi(conn.port->ptr),
	                                       (char *)NULL,
	                                       0);
	if (!res) {
		addReplyError(c, "mysql connect failed.");
		return NULL;
	}
	redisLog(REDIS_DEBUG, "mysql: connected.");

	return mysql;
}

void myredis_disconnect(MYSQL *mysql) {
	if (mysql) {
		mysql_close(mysql);
	}
}

MYSQL_RES *myredis_query_exec(redisClient *c, MYSQL *mysql, robj *q) {
	int s = mysql_query(mysql, (const char *)q->ptr);
	if (s) {
		addReplyError(c, "mysql: query failed");
		return NULL;
	}

	return mysql_store_result(mysql);
}

MYSQL_RES *myredis_query(redisClient *c, MYSQL *mysql) {
	robj *q = lookupKeyRead(c->db, c->argv[2]);
	if (!q) {
		addReplyError(c, "mysql: query key could not found.");
		return NULL;
	}

	MYSQL_RES *res = myredis_query_exec(c, mysql, q);
	if (!res)
		return NULL;

	int r_len = mysql_num_rows(res);
	int c_len = mysql_num_fields(res);

	addReplyMultiBulkLen(c, r_len*c_len);

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res))) {
		for(int i=0; i<c_len; i++) {
			addReplyBulkCString(c, row[i]);
		}
	}

	return res;
}

robj *myredis_query_scalar(redisClient *c, MYSQL *mysql) {
	robj *q = lookupKeyRead(c->db, c->argv[2]);
	if (!q) {
		addReplyError(c, "mysql: query key could not found.");
		return NULL;
	}

	MYSQL_RES *res = myredis_query_exec(c, mysql, q);
	if (!res)
		return NULL;

	if (res->row_count == 0)
		return createStringObject(REDIS_STRING, (size_t)NULL);

	MYSQL_ROW row  = mysql_fetch_row(res);

	return createStringObject(row[0], strlen(row[0]));
}
