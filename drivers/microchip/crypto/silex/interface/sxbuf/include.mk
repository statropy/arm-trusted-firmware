
ALLBUILDDIRS+=$(BUILDDIR)/interface/sxbuf/

SXBUFOBJS=$(BUILDDIR)/interface/sxbuf/sxbufops.o

$(BUILDDIR)/libsilexpk.so: $(COREOBJS) $(SXBUFOBJS) | builddir
	$(CC) $(LDFLAGS) --shared $(SXBUFOBJS) $(COREOBJS) -o $@

$(BUILDDIR)/libsilexpk.a: $(COREOBJS) $(SXBUFOBJS) | builddir
	$(AR) $(ARFLAGS) $@ $(SXBUFOBJS)  $(COREOBJS)

# For compatibility with Jenkins jobs
sxbuf: $(BUILDDIR)/libsilexpk.so

.PHONY: sxbuf
