#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <console.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/hello.h>

static void test_hello_callback ( char *data, size_t len ) {
	unsigned int i;
	char c;

	for ( i = 0 ; i < len ; i++ ) {
		c = data[i];
		if ( c == '\r' ) {
			/* Print nothing */
		} else if ( ( c == '\n' ) || ( c >= 32 ) || ( c <= 126 ) ) {
			putchar ( c );
		} else {
			putchar ( '.' );
		}
	}
}

void test_hello ( struct sockaddr_in *server, const char *message ) {
	struct hello_request hello;
	int rc;

	printf ( "Saying \"%s\" to %s:%d\n", message,
		 inet_ntoa ( server->sin_addr ), ntohs ( server->sin_port ) );
	
	memset ( &hello, 0, sizeof ( hello ) );
	hello.tcp.sin = *server;
	hello.message = message;
	hello.callback = test_hello_callback;

	rc = async_wait ( say_hello ( &hello ) );
	if ( rc ) {
		printf ( "HELLO fetch failed\n" );
	}
}