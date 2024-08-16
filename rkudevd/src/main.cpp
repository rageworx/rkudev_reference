/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <string>
#include <list>

#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////

#define DEVICE_NAME         "/dev/rkudev0"
#define BULK_BUFFER_SIZE    (1024*1024*5)

/////////////////////////////////////////////////////////////////////////

struct iostruct
{
    uint32_t        op_mode;
    uint32_t        usb_spd;
    char            verstr[32];
};

 struct buffstruct
 {
	 unsigned char* ptr;
	 unsigned length;
 };

/////////////////////////////////////////////////////////////////////////

static iostruct         iost = {0};
static int              fd_dev = -1;
static pthread_t        p_t_r;
static pthread_t        p_t_w;
static pthread_t        p_t_p;
static pthread_mutex_t  p_t_mutex;
static pthread_mutex_t  p_proc_mutex;
static unsigned char    datapool[ BULK_BUFFER_SIZE ] = {0};
static bool             keep_alived = true;
static bool             sighmtx = false;

////////////////////////////////////////////////////////////////////////

#define MIN_CMD_BUFFER_LEN			(5)
#define DEFAULT_TEST_BUFFER_LEN		(1024 * 1024 * 32) /// == 32MiB

// IO control set -
#define IO_SET_MODE 				_IOW(0xC8, 33, uint32_t)
#define IO_GET_MODE 				_IOR(0xC8, 34, uint32_t)
#define IO_GET_USB_SPEED			_IOR(0xC8, 35, uint32_t)
#define IO_GET_DRIVER_VERSION       _IOR(0xC8, 42, char*)

////////////////////////////////////////////////////////////////////////

list< buffstruct* > bufferList;
list< buffstruct* > writeBufferList;

////////////////////////////////////////////////////////////////////////

void addWriteBufferCmd( const char* cmd )
{
	if ( cmd != NULL )
	{
		buffstruct* pBS = new buffstruct;
		if ( pBS != NULL )
		{
			pBS->ptr = (unsigned char*)strdup( cmd );
            if ( pBS->ptr != NULL )
            {
                pBS->length = strlen( cmd );
                pthread_mutex_lock( &p_t_mutex );
                writeBufferList.push_back( pBS );
                pthread_mutex_unlock( &p_t_mutex );
                
                return;
            }

            delete pBS;
		}
	}
}

void* nullThread(void* data)
{
    printf( "null thread : %p\n", data );

    return data;
}

void* cmdProcThread(void* data)
{
    while ( keep_alived == true )
    {
		pthread_mutex_lock( &p_proc_mutex );
		size_t leftbl = bufferList.size();
		pthread_mutex_unlock( &p_proc_mutex );
		
		if ( leftbl > 0 )
		{
			pthread_mutex_lock( &p_proc_mutex );
			list< buffstruct* >::iterator it = bufferList.begin();
            buffstruct* pRef = *it;
            buffstruct* pBS = new buffstruct;
            if ( ( pBS != NULL ) && ( pRef->length > 0 ) )
            {
                pBS->ptr = new uint8_t[ pRef->length ];
                if ( pBS->ptr != NULL )
                {
                    memcpy( pBS->ptr, pRef->ptr, pRef->length );
                    pBS->length = pRef->length;
                }
                else
                {
                    delete pBS;
                    pBS = NULL;
                }
            }
            if ( pRef->ptr != NULL )
            {
                delete[] pRef->ptr;
            }
			bufferList.erase( it );
			pthread_mutex_unlock( &p_proc_mutex );

			if ( ( pBS != NULL ) && ( pBS->ptr != NULL ) )
			{
				if ( pBS->length > MIN_CMD_BUFFER_LEN )
				{
					string cmdCast = (const char*)pBS->ptr;

					if ( cmdCast.find( "HELLO" ) == 0 )
					{
						addWriteBufferCmd( "HOWAREYOU" );
					}
					else
					if ( cmdCast.find( "STATUS" ) == 0 )
					{
						char cmdmap[64] = {0};
						snprintf( cmdmap, 64, 
						          "STATUS:%u,%u,%zu,%u;",
								  (unsigned)iost.op_mode,
								  (unsigned)iost.usb_spd,
								  leftbl,
								  BULK_BUFFER_SIZE );
						addWriteBufferCmd( cmdmap );
					}
					else
					if ( cmdCast.find( "TESTFILL" ) == 0 )
					{
						size_t testbufflen = DEFAULT_TEST_BUFFER_LEN;

						if ( testbufflen > 0 )
						{
							size_t divloop = testbufflen / DEFAULT_TEST_BUFFER_LEN;
							size_t modloop = 0;
							
							if ( divloop == 0 ) 
							{
								divloop = 1;
								modloop = testbufflen;
							}
							else
							{
								modloop = testbufflen % DEFAULT_TEST_BUFFER_LEN;
							}

							char* fillbuff = new char[DEFAULT_TEST_BUFFER_LEN+1];
                            if ( fillbuff != NULL )
                            {
                                memset( fillbuff, 0, DEFAULT_TEST_BUFFER_LEN + 1 );

                                for( size_t cnt=0; cnt<divloop; cnt++  )
                                {
                                    if ( cnt+1 == divloop )
                                    {
                                        memset( fillbuff, 'A', modloop );
                                    }
                                    else
                                    {
                                        memset( fillbuff, 'A', DEFAULT_TEST_BUFFER_LEN );
                                    }
                                    
                                    addWriteBufferCmd( fillbuff );
                                }

                                delete[] fillbuff;
                            } /// of if (fillbuff != NULL )
						}
					}
					else
					if ( cmdCast.find( "RESET0" ) == 0 )
					{
						// not implemented yet.
						addWriteBufferCmd( "RETSET0:OK" );
					}
				} /// of i ( pBS != NULL ) ...
				
                delete[] pBS->ptr;
				delete pBS;
			}
		}
        else
        {
            usleep( 10 );
        }

    } /// of while( ... )

    pthread_exit( 0 );
    return NULL;
}

int procBuffer( const unsigned char* refbuff, unsigned reflen )
{
	if ( ( refbuff != NULL ) && ( reflen > 0 ) ) 
	{
		// duplicate buffer ...
		buffstruct* buffs = new buffstruct;
		if ( buffs != NULL )
		{
			buffs->ptr = new unsigned char[ reflen ];
			if ( buffs->ptr != NULL )
			{
				buffs->length = reflen;
				memcpy( buffs->ptr, refbuff, reflen );
				pthread_mutex_lock( &p_proc_mutex );
				bufferList.push_back( buffs );
				pthread_mutex_unlock( &p_proc_mutex );
				
				return 0;
			}
			
			// fail to release buffer.
			delete buffs;
		}
	}
	
	return -1;
}

void* readThread(void *data)
{
    if ( fd_dev < 0 )
    {
        printf( "device %s open failed as read\n", DEVICE_NAME );
        pthread_exit( 0 );
        return NULL;
    }

    // loop !
    while ( keep_alived == true )
    {
        pthread_mutex_lock( &p_t_mutex );
        int retlength = read(fd_dev, datapool, BULK_BUFFER_SIZE);

        if ( retlength > 0 )
        {
            printf( ">%s\n", (const char*) datapool );

            unsigned plen = (unsigned)retlength;
			
			int rpb = procBuffer( datapool, retlength );
			if ( rpb < 0 )
			{
				fprintf( stderr, 
                         "Warning : failed to processing buffer.\n" );
			}
			
			memset( datapool, 0, plen );
        }
		
        pthread_mutex_unlock( &p_t_mutex );
    }

    pthread_exit( 0 );
    return NULL;
}

void * writeThread(void *data)
{
    int retlength = 0;

    if ( fd_dev < 0 )
    {
        printf( "device %s open failed as write\n", DEVICE_NAME );
        pthread_exit( 0 );
        return NULL;
    }

    while ( keep_alived == true )
    {
		// check it again for fast escape.
        if ( keep_alived == false )
            break;

		buffstruct* pBS = NULL;
		
        pthread_mutex_lock( &p_t_mutex );
		if ( writeBufferList.size() > 0 )
		{
			list< buffstruct* >::iterator it = writeBufferList.begin();
			buffstruct* pRef = *it;
            if ( ( pRef != NULL ) && 
                 ( pRef->ptr != NULL ) && ( pRef->length > 0 ) )
            {
                pBS = new buffstruct;
                if ( pBS != NULL )
                {
                    pBS->ptr = new uint8_t[ pRef->length ];
                    if ( pBS->ptr != NULL )
                    {
                        memcpy( pBS->ptr, pRef->ptr, pRef->length );
                        pBS->length = pRef->length;
                    }
                }

                delete[] pRef->ptr;
            }
			writeBufferList.pop_front();			
		}
        pthread_mutex_unlock( &p_t_mutex );

		if ( pBS != NULL )
		{
            if ( ( pBS->ptr != NULL ) && ( pBS->length > 0 ) )
            {
                printf( " .. writing ( %u bytes -> %s ) : ", 
                        pBS->length,
                        (const char*) pBS->ptr );

                retlength = write( fd_dev, pBS->ptr, pBS->length );
                
                printf( "%d bytes done.\n", retlength );

                delete[] pBS->ptr;
            }

			delete pBS;
		}
    }

    pthread_exit( 0 );
    return NULL;
}

void sigHandle( int sig )
{
    if ( sighmtx == true )
        return;

    sighmtx = true;

    printf( "\n[!]Signal %d handling ...\n", sig );

    if ( keep_alived == true )
    {
        keep_alived = false;
	
        // wait for write, read threads ..
        pthread_join( p_t_r, NULL );
        pthread_join( p_t_w, NULL );

        // wait for processing thread ..
        pthread_cancel( p_t_p );
        pthread_join( p_t_p, NULL );

        if ( fd_dev >= 0 )
        {
            // set to passive write/read mode.
            iost.op_mode = 1;
            ioctl( fd_dev, IO_SET_MODE, &iost.op_mode );
        }
        
        close( fd_dev );

        exit(0);
    }
}

int main(int argc, char *argv[])
{
    printf( "RaphK's rk3399 series rkudev standard daemon/service, (C)2024 Raph.K\n" );

    int ptcret = 0;

    signal( SIGHUP, sigHandle );
    signal( SIGINT, sigHandle );

    fd_dev = open( DEVICE_NAME, O_RDWR );

    if ( fd_dev >= 0 )
    {
        printf( "device : %d, controlling user mode : ", fd_dev );
        // set to normal write/read mode.
        iost.op_mode = 0;
        if ( ioctl( fd_dev, IO_SET_MODE, &iost.op_mode ) == 0 )
            printf( "Ok.\n" );
        else
            printf( "Failure.\n" );

        // get connection speed ..
        ioctl( fd_dev, IO_GET_USB_SPEED, &iost.usb_spd );
        printf( "device connected speed value = %u.\n", iost.usb_spd );

        // get driver version
        ioctl( fd_dev, IO_GET_DRIVER_VERSION, &iost.verstr[0] );
        printf( "device drvier version : %s\n", iost.verstr );
    }
    else
    {
        printf( "cannot open device, not existed or not sudoer.\n" );
        goto emergencyexit;
    }
    
    pthread_mutex_init( &p_t_mutex, NULL );
    pthread_mutex_init( &p_proc_mutex, NULL );

    printf( " .. starting command processing thread : " );
    ptcret = pthread_create( &p_t_p, 0, cmdProcThread, &iost );
    if ( ptcret != 0 )
    {
        fprintf( stderr, 
                 "Error : buffer processing thread not started.\n" );
    }
    else
    {
        printf( "Ok.\n" );
    }

    printf( " .. starting reading thread : " );
    ptcret = pthread_create( &p_t_r, 0, readThread, 0 );
    if ( ptcret == 0 )
    {  
        printf( "Ok.\n" );
        printf( " .. starting reading thread : " );
        ptcret = pthread_create( &p_t_w, 0, writeThread, 0 );
        if ( ptcret != 0 )
        {
            fprintf( stderr,
                     "Error : cannot processing thread for read.\n" );
            keep_alived = false;
            pthread_join( p_t_r, NULL );
            goto emergencyexit;
        }
        printf( "Ok.\n" );
    }
    else
    {
        printf( "Failure !\n" );
        goto emergencyexit;
    }

    printf( "waiting server done...\n" );

    // wait for write, read threads ..
    pthread_join( p_t_r, NULL );
    pthread_join( p_t_w, NULL );

	// wait for processing thread ..
	pthread_cancel( p_t_p );
	pthread_join( p_t_p, NULL );

emergencyexit:
    if ( fd_dev >= 0 )
    {
        // set to passive write/read mode.
        iost.op_mode = 1;
        ioctl( fd_dev, IO_SET_MODE, &iost.op_mode );
    }
	
    close( fd_dev );

    printf( "\n" );

    return 0;
}

