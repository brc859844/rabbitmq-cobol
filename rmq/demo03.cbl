        identification division.
        program-id.    demo03.
        data division.
        working-storage section.

        01 rv                   binary-long.
        01 len                  binary-long.

        01 url                  pic x(50) value "amqp://guest:guest@10.10.116.196:5672".
        01 exchange             pic x(50) value "cobol-exchange".
        01 exchange-type        pic x(50) value "direct".
        01 binding-key          pic x(50) value "cobol-key".
        01 queue-name           pic x(50) value "cobol-queue".

        01 passive              binary-long value 0.
        01 durable              binary-long value 1.
        01 exclusive-flag       binary-long value 0.
        01 auto-delete          binary-long value 0.

        01 error-text           pic x(100).

        01 conn                 usage pointer.


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

        call "RMQ_DECLARE_EXCHANGE" using
                        by value conn
                        by reference exchange
                        by value 14
                        by reference exchange-type
                        by value 6
                        by value passive
                        by value durable
                        by value 0
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

        call "RMQ_DECLARE_QUEUE" using
                        by value conn
                        by reference queue-name
                        by value 11
                        by value 0
                        by value 0
                        by value passive
                        by value durable
                        by value exclusive-flag
                        by value auto-delete
                        by value 0
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


        call "RMQ_BIND_QUEUE" using
                        by value conn
                        by reference queue-name
                        by value 11
                        by reference exchange
                        by value 14
                        by reference binding-key
                        by value 9
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

end program demo03.
