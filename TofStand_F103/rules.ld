/* Stack rules inclusion.*/
INCLUDE ../rules_stacks.ld

/* Code rules inclusion.*/
INCLUDE ../rules_code.ld

/* Data rules inclusion.*/
INCLUDE ../rules_data.ld

/***************************************
 *  Here is place to read and write to *
 ***************************************/
SECTIONS {
    MSDStorage 0x08020000 :
    {
        *(MSDStorage)
    } > flash0
}
