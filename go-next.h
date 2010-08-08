"\n"
"function html_decode (s) {\n"
"	var from = new Array(/&gt;/g, /&lt;/g);\n"
"	var to = new Array(\">\", \"<\");\n"
"	for (var i = 0; i < from.length; i++) {\n"
"		s = s.replace(from[i], to[i]);\n"
"	}\n"
"	from = s.match(/&#[0-9]{1,5};/g);\n"
"	if (from)\n"
"		for (var i = 0; i < from.length; i++) {\n"
"			var n = parseInt(from[i].substr(2, from[i].length - 3));\n"
"			if (n)\n"
"				s = s.replace(new RegExp(from[i], 'g'), String.fromCharCode(n));\n"
"		}\n"
"	return s;\n"
"}\n"
"\n"
"\n"
"\n"
"//Make onlick-links \"clickable\"\n"
"HTMLElement.prototype.click = function() {\n"
"	try {\n"
"		if (typeof this.onclick == 'function') {\n"
"			return this.onclick({\n"
"				type: 'click'\n"
"			});\n"
"		} else\n"
"			return true;\n"
"	} catch(e) {\n"
"		return true;\n"
"	}\n"
"};\n"
"\n"
"var direction = '%s';\n"
"var links = document.links;\n"
"\n"
"var re = new Array();\n"
"\n"
"if (direction == 'next')\n"
"	re = new Array(/>/, />>/, />>>/, /^next$/i, /next *[>»]/i, /^далее/i, /^след[.у]/i, /next/i);\n"
"else if (direction == 'prev')\n"
"	re = new Array(/</, /<</, /<<</, /^prev$/i, /^previous/i, /prev[.]? *[>»]/i, /previous *[>»]/i, /^назад/i, /^пред[.ы]/i, /previous/i);\n"
"\n"
"function go() {\n"
"	for (var r = 0; r < re.length; r++) {\n"
"		var ri = re[r];\n"
"		for (var l = 0; l < links.length; l++) {\n"
"			var li = links[l];\n"
"			if (html_decode(li.innerHTML.replace(/<[^>]*>/g, '')).match(ri) != null) {\n"
"				li.focus();\n"
"				if (li.click() != false)\n"
"					document.location = li.href;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"go();\n"
"\n"
"\n"
