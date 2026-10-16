/* Keep the generated ZIP in its own flash partition, ahead of generic rodata. */
SECTION_PROLOGUE(xedge_resources,,)
{
    __xedge_resources_start = .;
    KEEP(*XedgeZip.c.obj(.rodata.cspPages))
    __xedge_resources_end = .;
} GROUP_LINK_IN(XEDGE_RESOURCES)

ASSERT(__xedge_resources_end > __xedge_resources_start,
       "Generated Xedge ZIP was not found; check XedgeZip.c and its section name")
ASSERT(__xedge_resources_start == ORIGIN(XEDGE_RESOURCES),
       "Xedge ZIP must start at the resource partition offset")
