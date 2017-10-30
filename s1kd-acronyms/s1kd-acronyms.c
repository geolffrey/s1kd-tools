#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxml/debugXML.h>
#include "stylesheets.h"

#define PROG_NAME "s1kd-acronyms"

/* Paths to text nodes where acronyms may occur */
#define ACRO_MARKUP_XPATH BAD_CAST "//para/text()"

bool prettyPrint = false;
int minimumSpaces = 2;
enum xmlFormat { BASIC, DEFLIST, TABLE } xmlFormat = BASIC;

void combineAcronymLists(xmlNodePtr dst, xmlNodePtr src)
{
	xmlNodePtr cur;

	for (cur = src->children; cur; cur = cur->next) {
		if (strcmp((char *) cur->name, "acronym") == 0) {
			xmlAddChild(dst, xmlCopyNode(cur, 1));
		}
	}
}

void findAcronymsInFile(xmlNodePtr acronyms, const char *path)
{
	xmlDocPtr doc, styleDoc, result;
	xsltStylesheetPtr style;

	doc = xmlReadFile(path, NULL, 0);

	styleDoc = xmlReadMemory((const char *) stylesheets_acronyms_xsl, stylesheets_acronyms_xsl_len, NULL, NULL, 0);

	style = xsltParseStylesheetDoc(styleDoc);

	result = xsltApplyStylesheet(style, doc, NULL);

	xmlFreeDoc(doc);
	xsltFreeStylesheet(style);

	combineAcronymLists(acronyms, xmlDocGetRootElement(result));

	xmlFreeDoc(result);
}

xmlDocPtr removeNonUniqueAcronyms(xmlDocPtr doc)
{
	xmlDocPtr styleDoc, result;
	xsltStylesheetPtr style;

	styleDoc = xmlReadMemory((const char *) stylesheets_unique_xsl, stylesheets_unique_xsl_len, NULL, NULL, 0);
	style = xsltParseStylesheetDoc(styleDoc);
	result = xsltApplyStylesheet(style, doc, NULL);
	xmlFreeDoc(doc);
	xsltFreeStylesheet(style);

	return result;
}

xmlNodePtr findChild(xmlNodePtr parent, const char *name)
{
	xmlNodePtr cur;

	for (cur = parent->children; cur; cur = cur->next) {
		if (strcmp((char *) cur->name, name) == 0) {
			return cur;
		}
	}

	return NULL;
}

int longestAcronymTerm(xmlNodePtr acronyms)
{
	xmlNodePtr cur;
	int longest = 0;

	for (cur = acronyms->children; cur; cur = cur->next) {
		if (strcmp((char *) cur->name, "acronym") == 0) {
			char *term;
			int len;

			term = (char *) xmlNodeGetContent(findChild(cur, "acronymTerm"));
			len = strlen(term);
			xmlFree(term);

			longest = len > longest ? len : longest;
		}
	}

	return longest;
}

void printAcronyms(xmlNodePtr acronyms, const char *path)
{
	xmlNodePtr cur;

	int longest = 0;

	FILE *out;

	if (strcmp(path, "-") == 0)
		out = stdout;
	else
		out = fopen(path, "w");
	
	if (prettyPrint)
		longest = longestAcronymTerm(acronyms);

	for (cur = acronyms->children; cur; cur = cur->next) {
		if (strcmp((char *) cur->name, "acronym") == 0) {
			char *term = (char *) xmlNodeGetContent(findChild(cur, "acronymTerm"));
			char *defn = (char *) xmlNodeGetContent(findChild(cur, "acronymDefinition"));

			char *type = (char *) xmlGetProp(cur, BAD_CAST "acronymType");

			if (prettyPrint) {
				int len, nspaces, i;

				len = strlen(term);

				nspaces = longest - len + minimumSpaces;

				fprintf(out, "%s", term);
				for (i = 0; i < nspaces; ++i) {
					fputc(' ', out);
				}
				if (type) {
					fprintf(out, "%s", type);
				} else {
					fprintf(out, "    ");
				}
				for (i = 0; i < minimumSpaces; ++i) {
					fputc(' ', out);
				}
				fprintf(out, "%s\n", defn);
			} else {
				fprintf(out, "%s\t", term);
				if (type) {
					fprintf(out, "%s\t", type);
				} else {
					fprintf(out, "    \t");
				}
				fprintf(out, "%s\n", defn);
			}

			xmlFree(term);
			xmlFree(defn);
			xmlFree(type);
		}
	}

	if (out != stdout)
		fclose(out);
}

xmlDocPtr formatXmlAs(xmlDocPtr doc, unsigned char *src, unsigned int len)
{
	xmlDocPtr styleDoc, result;
	xsltStylesheetPtr style;

	styleDoc = xmlReadMemory((const char *) src, len, NULL, NULL, 0);

	style = xsltParseStylesheetDoc(styleDoc);

	result = xsltApplyStylesheet(style, doc, NULL);

	xmlFreeDoc(doc);
	xsltFreeStylesheet(style);

	return result;
}

xmlDocPtr limitToTypes(xmlDocPtr doc, const char *types)
{
	xmlDocPtr styleDoc, result;
	xsltStylesheetPtr style;
	const char *params[3];
	char *typesParam;

	typesParam = malloc(strlen(types) + 3);
	sprintf(typesParam, "'%s'", types);

	params[0] = "types";
	params[1] = typesParam;
	params[2] = NULL;

	styleDoc = xmlReadMemory((const char *) stylesheets_types_xsl, stylesheets_types_xsl_len, NULL, NULL, 0);

	style = xsltParseStylesheetDoc(styleDoc);

	result = xsltApplyStylesheet(style, doc, params);

	free(typesParam);

	xmlFreeDoc(doc);
	xsltFreeStylesheet(style);

	return result;
}

void markupAcronymInNode(xmlNodePtr node, xmlNodePtr acronym)
{
	xmlChar *content;
	xmlChar *term = NULL;
	int termLen, contentLen;
	xmlNodePtr cur;
	int i;

	for (cur = acronym->children; cur; cur = cur->next)
		if (xmlStrcmp(cur->name, BAD_CAST "acronymTerm") == 0)
			term = xmlNodeGetContent(cur);

	termLen = xmlStrlen(term);

	content = xmlNodeGetContent(node);
	contentLen = xmlStrlen(content);

	i = 0;
	while (i < contentLen) {
		if (xmlStrcmp(xmlStrsub(content, i, termLen), term) == 0) {
			xmlChar *s1 = xmlStrndup(content, i);
			xmlChar *s2 = xmlStrdup(xmlStrsub(content, i + termLen, xmlStrlen(content)));
			xmlNodePtr acr;

			xmlFree(content);

			xmlNodeSetContent(node, s1);
			acr = xmlAddNextSibling(node, xmlCopyNode(acronym, 1));
			xmlAddNextSibling(acr, xmlNewText(s2));

			xmlFree(s1);
			
			content = s2;
			contentLen = xmlStrlen(s2);
			i = 0;
		} else {
			++i;
		}
	}

	xmlFree(term);
	xmlFree(content);
}

void markupAcronyms(xmlDocPtr doc, xmlNodePtr acronyms)
{
	xmlNodePtr cur;

	for (cur = acronyms->children; cur; cur = cur->next) {
		if (xmlStrcmp(cur->name, BAD_CAST "acronym") == 0) {
			xmlXPathContextPtr ctx;
			xmlXPathObjectPtr obj;

			ctx = xmlXPathNewContext(doc);

			obj = xmlXPathEvalExpression(ACRO_MARKUP_XPATH, ctx);

			if (!xmlXPathNodeSetIsEmpty(obj->nodesetval)) {
				int i;

				for (i = 0; i < obj->nodesetval->nodeNr; ++i) {
					markupAcronymInNode(obj->nodesetval->nodeTab[i], cur);
				}
			}
		}
	}
}

void markupAcronymsInFile(const char *path, xmlNodePtr acronyms, const char *out)
{
	xmlDocPtr doc;

	doc = xmlReadFile(path, NULL, 0);

	markupAcronyms(doc, acronyms);

	xmlSaveFile(out, doc);

	xmlFreeDoc(doc);
}

void showHelp(void)
{
	puts("Usage: " PROG_NAME " [-pxdth?] [-n <#>] [-T <types>] [-o <file>] [<datamodules>]");
	puts("");
	puts("Options:");
	puts("  -p          Pretty print text/XML output");
	puts("  -n <#>      Minimum spaces after term in pretty printed output");
	puts("  -x          Output XML instead of text");
	puts("  -d          Format XML output as definitionList");
	puts("  -t          Format XML output as table");
	puts("  -T <types>  Only search for acronyms of these types");
	puts("  -o <file>   Output to <file> instead of stdout");
	puts("  -m <list>   Add markup for acronyms");
	puts("  -h -?  Show usage message");
}

int main(int argc, char **argv)
{
	int i;

	xmlDocPtr doc;
	xmlNodePtr acronyms;

	bool xmlOut = false;
	char *types = NULL;
	char *out = strdup("-");
	bool outarg = false;
	char *markup = NULL;

	while ((i = getopt(argc, argv, "pn:xdtT:o:m:h?")) != -1) {
		switch (i) {
			case 'p':
				prettyPrint = true;
				break;
			case 'n':
				minimumSpaces = atoi(optarg);
				break;
			case 'x':
				xmlOut = true;
				break;
			case 'd':
				xmlFormat = DEFLIST;
				break;
			case 't':
				xmlFormat = TABLE;
				break;
			case 'T':
				types = strdup(optarg);
				break;
			case 'o':
				outarg = true;
				free(out);
				out = strdup(optarg);
				break;
			case 'm':
				markup = strdup(optarg);
				break;
			case 'h':
			case '?':
				showHelp();
				exit(0);
		}
	}


	if (markup) {
		doc = xmlReadFile(markup, NULL, 0);
		acronyms = xmlDocGetRootElement(doc);

		if (optind >= argc) {
			markupAcronymsInFile("-", acronyms, out);
		}

		for (i = optind; i < argc; ++i) {
			if (outarg) {
				markupAcronymsInFile(argv[i], acronyms, out);
			} else {
				markupAcronymsInFile(argv[i], acronyms, argv[i]);
			}
		}
	} else {
		doc = xmlNewDoc(BAD_CAST "1.0");
		acronyms = xmlNewNode(NULL, BAD_CAST "acronyms");
		xmlDocSetRootElement(doc, acronyms);

		if (optind >= argc) {
			findAcronymsInFile(acronyms, "-");
		}

		for (i = optind; i < argc; ++i) {
			findAcronymsInFile(acronyms, argv[i]);
		}

		doc = removeNonUniqueAcronyms(doc);

		if (types)
			doc = limitToTypes(doc, types);

		if (xmlOut) {
			switch (xmlFormat) {
				case DEFLIST:
					doc = formatXmlAs(doc, stylesheets_list_xsl, stylesheets_list_xsl_len);
					break;
				case TABLE:
					doc = formatXmlAs(doc, stylesheets_table_xsl, stylesheets_table_xsl_len);
					break;
				default:
					break;
			}

			if (prettyPrint) {
				xmlSaveFormatFile(out, doc, 1);
			} else {
				xmlSaveFile(out, doc);
			}
		} else {
			printAcronyms(xmlDocGetRootElement(doc), out);
		}
	}

	free(types);
	free(out);
	free(markup);

	xmlFreeDoc(doc);

	xsltCleanupGlobals();
	xmlCleanupParser();

	return 0;
}
