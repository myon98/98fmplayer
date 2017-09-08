INCS:=copy.ps.inc color.ps.inc color_trans.ps.inc key.ps.inc fill.ps.inc font.ps.inc blit.vs.inc
FXC:=env WINEDEBUG=-all wine $(HOME)/hlsl/fxc/fxc
all:	$(INCS)

%.inc:	%.inc.tmp
	dos2unix < $< | sed -e 's/^const BYTE g_/static const BYTE /' -e '$$a\\' > $@

%.vs.inc.tmp: %.vs.hlsl
	$(FXC) /nologo /O3 /Tvs_2_0 /E$(basename $(basename $<)) /Fh $@ $<

%.ps.inc.tmp: %.ps.hlsl
	$(FXC) /nologo /O3 /Tps_2_0 /E$(basename $(basename $<)) /Fh $@ $<

clean:
	rm -f $(INCS)
