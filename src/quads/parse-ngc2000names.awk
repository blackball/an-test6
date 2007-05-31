BEGIN{
#print "#include \"an-bool.h\"";
#print "struct ngc_name {";
#print "  bool is_ngc;";
#print "  int id;";
#print "  char* name;";
#print "};\n";
#print "typedef struct ngc_name ngc_name;";
#print "struct ngc_name ngcnames[] = {";
}
{
namepadded = substr($0, 1, 35);
split(namepadded, parts, " ");
name = "";
#for (p in parts) {
for (i=1;; i++) {
	if (!(i in parts)) {
		break;
	}
	if (i > 1) {
		name = name " ";
	}
	name = name parts[i];
	#name = (i == (name == "") ? "" : (name " ")) " " parts[i];
	#name = ((name == "") ? "" : (name " ")) " " parts[i];
	#name = ((name == "") ? "" : (name " ")) " " parts[p];
}

ic = substr($0, 37, 1);
isic = (ic == "I");
isngc = !isic;

idpad = substr($0, 38, 4);
split(idpad, parts, " ");
 id = parts[1];

#print "** " name " **" (isic ? "IC" : "NGC") " " id;

 if (id > 0) {
	 print "{" " .is_ngc = " (isngc ? "TRUE" : "FALSE") ",";
	 print "  .id = " id ",";
	 print "  .name = \"" name "\"";
	 print "},";
 }

}
END{
#print "};\n";
}
