        identification division.
        program-id.    demo05.
        data division.
        working-storage section.

        77 AMQP_BASIC_DELIVERY_MODE_FLAG binary-long value 4096.
        77 AMQP_BASIC_CONTENT_TYPE_FLAG binary-long value 32768.

        01 rv                   binary-long.
        01 len                  binary-long.

        01 url                  pic x(50) value "amqp://guest:guest@10.10.116.196:5672".
        01 exchange             pic x(50) value "amq.direct".
        01 routing-key          pic x(50) value "test-key".
        01 msg                  pic x(50) value "A test message".
        01 content-type         pic x(50) value "text/plain".

        01 delivery-mode        binary-char value 2.
        01 error-text           pic x(100).

        01 conn                 usage pointer.
        01 props                usage pointer.


        procedure division.

        move length of url to len.
        call "RMQ_CONNECT" using
                        by reference conn
                        by reference url
                        by value len
                        giving rv.

        if rv = 0
           call "RMQ_STRERROR" using
                        by value 0
                        by reference error-text
                        by value 50
           end-call

           display error-text
           stop run
        end-if.

        call "RMQ_PROPS_NEW" giving props.

        call "RMQ_PROPS_SET" using
                        by value props
                        by value AMQP_BASIC_DELIVERY_MODE_FLAG
                        by reference delivery-mode
                        by value 0.

        call "RMQ_PROPS_SET" using
                        by value props
                        by value AMQP_BASIC_CONTENT_TYPE_FLAG
                        by reference content-type
                        by value 10.

        call "RMQ_PUBLISH" using
                        by value conn
                        by reference exchange
                        by value 10
                        by reference routing-key
                        by value 8
                        by value 0
                        by value 0
                        by reference msg
                        by value 14
                        by value props
                        giving rv.

        if rv = 0
           call "RMQ_STRERROR" using
                        by value conn
                        by reference error-text
                        by value 50
           end-call

           display error-text
           stop run
        end-if.

        call "RMQ_PROPS_FREE" using by value props.
        call "RMQ_DISCONNECT" using by value conn.
        stop run.

end program demo05.
