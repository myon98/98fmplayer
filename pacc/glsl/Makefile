VSHADERS:=blit.vert
FSHADERS:=copy.frag color.frag color_trans.frag key.frag fill.frag font.frag
DSHEADER:=dsheader
ESHEADER:=esheader
INCS:=$(addsuffix .inc,$(VSHADERS) $(FSHADERS) $(DSHEADER) $(ESHEADER))
SHADERS2:=$(addsuffix .ds.vert,$(basename $(VSHADERS)))
SHADERS2+=$(addsuffix .es.vert,$(basename $(VSHADERS)))
SHADERS2+=$(addsuffix .ds.frag,$(basename $(FSHADERS)))
SHADERS2+=$(addsuffix .es.frag,$(basename $(FSHADERS)))
VALS:=$(addsuffix .vald,$(SHADERS2))
VALIDATOR:=glslangValidator
VFLAGS:=

all:	$(INCS)

val:	$(VALS)

%.es.vert:	%.vert
	cat $(ESHEADER) $< > $@

%.es.frag:	%.frag
	cat $(ESHEADER) $< > $@

%.ds.vert:	%.vert
	cat $(DSHEADER) $< > $@

%.ds.frag:	%.frag
	cat $(DSHEADER) $< > $@

%.vald:	%
	$(VALIDATOR) $(VFLAGS) $<

%.inc:	%.zt
	xxd -i $< | sed -e '/^unsigned char /c\static const uint8_t $(shell echo $(basename $<) | sed -e 's/\./_/g')[] = {' -e '/^unsigned int /c\\' > $@

%.zt:	%
	dd bs=1 count=1 if=/dev/zero | cat $< - > $@

clean:
	rm -f $(INCS)
