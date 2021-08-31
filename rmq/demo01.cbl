        identification division.
        program-id.    demo01.
        data division.
        working-storage section.

        01 rv                   binary-long.

        01 url                  pic x(80) value "amqp://guest:guest@10.10.116.196:5672".
        01 exchange             pic x(50) value "amq.direct".
        01 routing-key          pic x(50) value "test-key".
        01 msg                  pic x(50) value "A test message".

        01 error-text           pic x(100).

        01 conn                 usage pointer.
        01 len                  pic 9(9) comp.


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
                        by value 0
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

        
        call "RMQ_DISCONNECT" using by value conn.
        stop run.

        end program demo01.

