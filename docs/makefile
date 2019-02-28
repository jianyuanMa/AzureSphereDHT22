
DEFAULT: all


OUTDIR = out
DOC_ROOT = .
DOC_TOOL_PATH = ../tools/documents
ECHO     = echo
CP       = cp
MV       = mv

include $(DOC_TOOL_PATH)/doc.mk


all:

clean:
	rm -rf $(OUTDIR)


###############################################################################
# targets for document generation

#doc: asure sphere

#% : $(OUTDIR)/%.pdf

define AddDocument
# $1 document name

$1 : $(OUTDIR)/$1.pdf

$(OUTDIR)/$1.pdf : $(OUTDIR)/$1/main.pdf
	@$(ECHO) Generating $$@
	@$(CP) $$< $$@
	@$(MV) $$@ .



all: $(OUTDIR)/$1.pdf

endef

$(eval $(call AddDocument,asdoc))