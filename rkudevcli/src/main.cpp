#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <libusb.h>

#define DEV_USB_VID     (0x2207)
#define DEV_USB_PID     (0x0019)
#define EP_IN           (0x81)
#define EP_OUT          (0x01)

static libusb_context* lusbctx = NULL;

int u_write( libusb_device_handle* d, const char* s )
{
    if ( ( d != NULL ) && ( s != NULL ) )
    {
        int slen = strlen( s );
        int tsz  = 0;
        int r = LIBUSB_ERROR_IO;
        int rt = 0;

        while( r != LIBUSB_SUCCESS )
        {
            r  = \
            libusb_bulk_transfer( d, EP_OUT,
                                  (uint8_t*)s,
                                  slen + 1,
                                  &tsz,
                                  1000 );

            if ( r != LIBUSB_SUCCESS )
            {
                rt++;
                printf( "." );
                fflush( stdout );

                if ( rt > 10 )
                    break;

                usleep( 25000 );
            }
        }

        return r;
    }

    return -1;
}

void test_dev( libusb_device_handle* dev )
{
    if ( dev != nullptr )
    {
        printf ( "Sending hello " );
        fflush( stdout );

        char strHello[] = "HELLO  ";
        int r = u_write( dev, strHello );

        if ( r == LIBUSB_SUCCESS )
        {
            printf( "Ok.\n" );
        }
        else
        {
            fprintf( stderr, "Failure(%d)\n", r );
            return;
        }

        printf( "Waiting ack ... " );
        fflush( stdout );

        uint8_t rcvb[32] = {0};
        int tsz = 0;
        r = LIBUSB_ERROR_IO;
        int rr = 0;

        while( r != LIBUSB_SUCCESS )
        {
            r = libusb_bulk_transfer( dev, EP_IN,
                                      rcvb, 32, &tsz, 10000 );
            if ( rr != LIBUSB_SUCCESS )
            {
                rr++;
                if ( rr > 10 )
                    break;
                usleep( 25000 );
            }
        }

        if ( r != LIBUSB_SUCCESS )
        {
            fprintf( stderr, "Failed to recv HELLO.\n" );
            return;
        }

        printf( "recv> %s\n", (const char*)rcvb );
    }
}

int main( int argc, char** argv )
{
    libusb_init( &lusbctx );

    libusb_device_handle* devh = NULL;
    libusb_device** devlist = NULL;

    size_t devcnt = libusb_get_device_list( lusbctx, &devlist );

    for( size_t cnt=0; cnt<devcnt; cnt++ )
    {
        libusb_device* dev = devlist[cnt];
        libusb_device_descriptor desc = {0};
        if ( libusb_get_device_descriptor( dev, &desc ) == 0)
        {
            printf( "[%3zu] %04X : %04X .. \n", cnt, 
                    desc.idVendor, desc.idProduct );

            if ( ( desc.idVendor == DEV_USB_VID ) &&
                 ( desc.idProduct == DEV_USB_PID ) )
            {
                int err = libusb_open( devlist[cnt], &devh );
                if ( err == 0 )
                {
                    printf( "device open : OK.\n" );

                    test_dev( devh );

                    libusb_close( devh );
                }
                else
                {
                    printf( "failed to open device !\n" );
                }
            }
        }
    }

    libusb_free_device_list( devlist, devcnt );
    libusb_exit( lusbctx );

    return 0;
}
