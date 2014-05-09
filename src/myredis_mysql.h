/*
 * myredis_mysql.h
 *
 *  Created on: 2014. 5. 9.
 *      Author: powerumc
 */

#ifndef MYREDIS_MYSQL_H_
#define MYREDIS_MYSQL_H_


#include "redis.h"
#include "sds.h"
#include "../deps/mysql/include/mysql.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MYREDIS_MYSQL_PORT 3306

typedef struct __myredis_conn {
	robj *host;
	robj *user;
	robj *passwd;
	robj *db;
	robj *port;
} myredis_conn;


void mysqlqCommand(redisClient *c);
void mysqlqsCommand(redisClient *c, MYSQL* mysql);
MYSQL *myredis_connect(redisClient *c);
MYSQL_RES *myredis_query(redisClient *c, MYSQL *mysql);
void myredis_disconnect(MYSQL *mysql);
robj *myredis_query_scalar(redisClient *c, MYSQL *mysql);



#endif /* MYREDIS_MYSQL_H_ */
