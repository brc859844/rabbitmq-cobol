        identification division.
        program-id.    my_svc1.
        data division.
        working-storage section.


        linkage section.

        01 ctxt                 usage pointer.
        01 idata                pic x(16384).
        01 ilen                 usage binary-long.
        01 odata                usage pointer.
        01 olen                 usage binary-long.

        01 txt 			pic x(60) based.

        procedure division using 
        	        by reference ctxt, 
        	        by reference idata, 
        	        by reference ilen, 
        	        by reference odata, 
        	        by reference olen.

        allocate (60) characters initialized returning odata.
        set address of txt to odata.
        move "Cool" to txt.
        move 4 to olen.

        *> There must be a better way in OpenCOBOL of handling idata, but I've not 
        *> found it yet
        *>
        display ilen.
        display idata(1:ilen).
        display "Hello from SVC1".


        end program my_svc1.


