SECTIONS
{
    . = 0x7c00;
    .text :
    {
        _ftext = .; /* program will be loaded to 0x7c00 */
    } = 0
}
