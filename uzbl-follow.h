"\n"
"// This script is taken from UZBL and edited a little\n"
"\n"
"/* This is the basic linkfollowing script.\n"
" * Its pretty stable, and you configure which keys to use for hinting\n"
" *\n"
" * TODO: Some pages mess around a lot with the zIndex which\n"
" * lets some hints in the background.\n"
" * TODO: Some positions are not calculated correctly (mostly\n"
" * because of uber-fancy-designed-webpages. Basic HTML and CSS\n"
" * works good\n"
" * TODO: Still some links can't be followed/unexpected things\n"
" * happen. Blame some freaky webdesigners ;)\n"
" */\n"
"\n"
"//Just some shortcuts and globals\n"
"var uzblid = 'uzbl_link_hint';\n"
"var uzbldivid = uzblid + '_div_container';\n"
"var doc = document;\n"
"var win = window;\n"
"var links = document.links;\n"
"var forms = document.forms;\n"
"//Make items \"clickable\"\n"
"HTMLElement.prototype.click = function(command) {\n"
"	return midorator_command(command, this);\n"
"};\n"
"\n"
"//Calculate element position to draw the hint\n"
"//Pretty accurate but on fails in some very fancy cases\n"
"function elementPosition(el) {\n"
"    var up = el.offsetTop;\n"
"    var left = el.offsetLeft;\n"
"    var width = el.offsetWidth;\n"
"    var height = el.offsetHeight;\n"
"    while (el.offsetParent) {\n"
"        el = el.offsetParent;\n"
"        up += el.offsetTop;\n"
"        left += el.offsetLeft;\n"
"    }\n"
"    return [up, left, width, height];\n"
"}\n"
"//Calculate if an element is visible\n"
"function isVisible(el) {\n"
"    if (el == doc) {\n"
"        return true;\n"
"    }\n"
"    if (!el) {\n"
"        return false;\n"
"    }\n"
"    if (!el.parentNode) {\n"
"        return false;\n"
"    }\n"
"    if (el.style) {\n"
"        if (el.style.display == 'none') {\n"
"            return false;\n"
"        }\n"
"        if (el.style.visibility == 'hidden') {\n"
"            return false;\n"
"        }\n"
"    }\n"
"    return isVisible(el.parentNode);\n"
"}\n"
"//Calculate if an element is on the viewport.\n"
"function elementInViewport(el) {\n"
"    offset = elementPosition(el);\n"
"    var up = offset[0];\n"
"    var left = offset[1];\n"
"    var width = offset[2];\n"
"    var height = offset[3];\n"
"    return up < window.pageYOffset + window.innerHeight && left < window.pageXOffset + window.innerWidth && (up + height) > window.pageYOffset && (left + width) > window.pageXOffset;\n"
"}\n"
"//Removes all hints/leftovers that might be generated\n"
"//by this script.\n"
"function removeAllHints() {\n"
"    var elements = doc.getElementById(uzbldivid);\n"
"    if (elements) {\n"
"        elements.parentNode.removeChild(elements);\n"
"    }\n"
"}\n"
"//Generate a hint for an element with the given label\n"
"//Here you can play around with the style of the hints!\n"
"function generateHint(el, label) {\n"
"    var pos = elementPosition(el);\n"
"    var hint = doc.createElement('div');\n"
"    hint.setAttribute('name', uzblid);\n"
"    hint.innerText = label;\n"
"    hint.style.display = 'inline';\n"
"    hint.style.backgroundColor = '#B9FF00';\n"
"    hint.style.border = '2px solid #4A6600';\n"
"    hint.style.color = 'black';\n"
"    hint.style.fontSize = '9px';\n"
"    hint.style.fontWeight = 'bold';\n"
"    hint.style.lineHeight = '9px';\n"
"    hint.style.margin = '0px';\n"
"    hint.style.width = 'auto'; // fix broken rendering on w3schools.com\n"
"    hint.style.padding = '1px';\n"
"    hint.style.position = 'absolute';\n"
"    hint.style.zIndex = '1000';\n"
"    // hint.style.textTransform = 'uppercase';\n"
"    hint.style.left = pos[1] + 'px';\n"
"    hint.style.top = pos[0] + 'px';\n"
"    // var img = el.getElementsByTagName('img');\n"
"    // if (img.length > 0) {\n"
"    //     hint.style.top = pos[1] + img[0].height / 2 - 6 + 'px';\n"
"    // }\n"
"    hint.style.textDecoration = 'none';\n"
"    // hint.style.webkitBorderRadius = '6px'; // slow\n"
"    // Play around with this, pretty funny things to do :)\n"
"    // hint.style.webkitTransform = 'scale(1) rotate(0deg) translate(-6px,-5px)';\n"
"    return hint;\n"
"}\n"
"//Here we choose what to do with an element if we\n"
"//want to \"follow\" it. On form elements we \"select\"\n"
"//or pass the focus, on links we try to perform a click,\n"
"//but at least set the href of the link. (needs some improvements)\n"
"function clickElem(item, command) {\n"
"	removeAllHints();\n"
"	midorator_command('hide entry');\n"
"	if (item) {\n"
"		item.click(command);\n"
"	}\n"
"}\n"
"//Returns a list of all links (in this version\n"
"//just the elements itself, but in other versions, we\n"
"//add the label here.\n"
"function addLinks() {\n"
"    res = [[], []];\n"
"    for (var l = 0; l < links.length; l++) {\n"
"        var li = links[l];\n"
"        if (isVisible(li) && elementInViewport(li)) {\n"
"            res[0].push(li);\n"
"        }\n"
"    }\n"
"    return res;\n"
"}\n"
"//Same as above, just for the form elements\n"
"function addFormElems() {\n"
"    res = [[], []];\n"
"    for (var f = 0; f < forms.length; f++) {\n"
"        for (var e = 0; e < forms[f].elements.length; e++) {\n"
"            var el = forms[f].elements[e];\n"
"            if (el && ['INPUT', 'TEXTAREA', 'SELECT'].indexOf(el.tagName) + 1 && isVisible(el) && elementInViewport(el)) {\n"
"                res[0].push(el);\n"
"            }\n"
"        }\n"
"    }\n"
"    return res;\n"
"}\n"
"//Draw all hints for all elements passed. \"len\" is for\n"
"//the number of chars we should use to avoid collisions\n"
"function reDrawHints(elems, chars) {\n"
"    removeAllHints();\n"
"    var hintdiv = doc.createElement('div');\n"
"    hintdiv.setAttribute('id', uzbldivid);\n"
"    for (var i = 0; i < elems[0].length; i++) {\n"
"        if (elems[0][i]) {\n"
"            var label = elems[1][i].substring(chars);\n"
"            var h = generateHint(elems[0][i], label);\n"
"            hintdiv.appendChild(h);\n"
"        }\n"
"    }\n"
"    if (document.body) {\n"
"        document.body.appendChild(hintdiv);\n"
"    }\n"
"}\n"
"// pass: number of keys\n"
"// returns: key length\n"
"function labelLength(n) {\n"
"	var oldn = n;\n"
"	var keylen = 0;\n"
"	if(n < 2) {\n"
"		return 1;\n"
"	}\n"
"	n -= 1; // our highest key will be n-1\n"
"	while(n) {\n"
"		keylen += 1;\n"
"		n = Math.floor(n / charset.length);\n"
"	}\n"
"	return keylen;\n"
"}\n"
"// pass: number\n"
"// returns: label\n"
"function intToLabel(n) {\n"
"	var label = '';\n"
"	do {\n"
"		label = charset.charAt(n %% charset.length) + label;\n"
"		n = Math.floor(n / charset.length);\n"
"	} while(n);\n"
"	return label;\n"
"}\n"
"// pass: label\n"
"// returns: number\n"
"function labelToInt(label) {\n"
"	var n = 0;\n"
"	var i;\n"
"	for(i = 0; i < label.length; ++i) {\n"
"		n *= charset.length;\n"
"		n += charset.indexOf(label[i]);\n"
"	}\n"
"	return n;\n"
"}\n"
"//Put it all together\n"
"function followLinks(follow, newtab) {\n"
"    // if(follow.charAt(0) == 'l') {\n"
"    //     follow = follow.substr(1);\n"
"    //     charset = 'thsnlrcgfdbmwvz-/';\n"
"    // }\n"
"    var s = follow.split('');\n"
"    var linknr = labelToInt(follow);\n"
"    var linkelems = addLinks();\n"
"    var formelems = addFormElems();\n"
"    var elems = [linkelems[0].concat(formelems[0]), linkelems[1].concat(formelems[1])];\n"
"    var len = labelLength(elems[0].length);\n"
"    var oldDiv = doc.getElementById(uzbldivid);\n"
"    var leftover = [[], []];\n"
"    if (s.length == len && linknr < elems[0].length && linknr >= 0) {\n"
"        clickElem(elems[0][linknr], newtab);\n"
"    } else {\n"
"        for (var j = 0; j < elems[0].length; j++) {\n"
"            var b = true;\n"
"            var label = intToLabel(j);\n"
"            var n = label.length;\n"
"            for (n; n < len; n++) {\n"
"                label = charset.charAt(0) + label;\n"
"            }\n"
"            for (var k = 0; k < s.length; k++) {\n"
"                b = b && label.charAt(k) == s[k];\n"
"            }\n"
"            if (b) {\n"
"                leftover[0].push(elems[0][j]);\n"
"                leftover[1].push(label);\n"
"            }\n"
"        }\n"
"        reDrawHints(leftover, s.length);\n"
"    }\n"
"}\n"
"\n"
"//Parse input: first argument is follow keys, second is user input.\n"
"var charset = '%s';\n"
"followLinks('%s', '%s');\n"
