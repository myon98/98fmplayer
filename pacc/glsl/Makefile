VSHADERS:=blit.vert
FSHADERS:=copy.frag color.frag color_trans.frag
DSHEADER:=dsheader
DS3HEADER:=ds3header
ESHEADER:=esheader
ES3HEADER:=es3header
INCS:=$(addsuffix .inc,$(VSHADERS) $(FSHADERS) $(DSHEADER) $(DS3HEADER) $(ESHEADER) $(ES3HEADER))
SHADERS2:=$(addsuffix .ds.vert,$(basename $(VSHADERS)))
SHADERS2:=$(addsuffix .ds3.vert,$(basename $(VSHADERS)))
SHADERS2+=$(addsuffix .es.vert,$(basename $(VSHADERS)))
SHADERS2+=$(addsuffix .es3.vert,$(basename $(VSHADERS)))
SHADERS2+=$(addsuffix .ds.frag,$(basename $(FSHADERS)))
SHADERS2+=$(addsuffix .ds3.frag,$(basename $(FSHADERS)))
SHADERS2+=$(addsuffix .es.frag,$(basename $(FSHADERS)))
SHADERS2+=$(addsuffix .es3.frag,$(basename $(FSHADERS)))
VALS:=$(addsuffix .vald,$(SHADERS2))
VALIDATOR:=glslangValidator
VFLAGS:=

all:	$(INCS)

val:	$(VALS)

%.es.vert:	%.vert
	cat $(ESHEADER) $< > $@

%.es.frag:	%.frag
	cat $(ESHEADER) $< > $@

%.es3.vert:	%.vert
	cat $(ES3HEADER) $< > $@

%.es3.frag:	%.frag
	cat $(ES3HEADER) $< > $@

%.ds.vert:	%.vert
	cat $(DSHEADER) $< > $@

%.ds.frag:	%.frag
	cat $(DSHEADER) $< > $@

%.ds3.vert:	%.vert
	cat $(DS3HEADER) $< > $@

%.ds3.frag:	%.frag
	cat $(DS3HEADER) $< > $@

%.vald:	%
	$(VALIDATOR) $(VFLAGS) $<

%.inc:	%.zt
	xxd -i $< | sed -e '/^unsigned char /c\static const uint8_t $(shell echo $(basename $<) | sed -e 's/\./_/g')[] = {' -e '/^unsigned int /c\\' > $@

%.zt:	%
	dd bs=1 count=1 if=/dev/zero | cat $< - > $@

clean:
	rm -f $(INCS)
