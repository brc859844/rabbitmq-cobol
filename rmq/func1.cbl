        identification division.
        program-id. func1.
        data division.
        working-storage section.

        linkage section.

        01 ctxt                 usage pointer.
        01 idata                usage pointer.
        01 ilen                 usage binary-long.
        01 odata                usage pointer.
        01 olen                 usage binary-long.


        procedure division using ctxt, idata, ilen, odata, olen.

        display idata(1:ilen).

        end program func1.
