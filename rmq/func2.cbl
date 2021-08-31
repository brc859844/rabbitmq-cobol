        identification division.
        program-id. func2.
        data division.
        working-storage section.

        linkage section.

        01 ctxt                 usage pointer.
        01 idata                usage pointer.
        01 ilen                 usage binary-long.
        01 odata                usage pointer.
        01 olen                 usage binary-long.

        01 txt                  pic x(60) based.

        procedure division using ctxt, idata, ilen, odata, olen.

        display idata(1:ilen).

        allocate (60) characters initialized returning odata.
        set address of txt to odata.
        move "This is the reply" to txt.
        move 17 to olen.

        end program func2.
