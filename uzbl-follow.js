
// This script is taken from UZBL and edited a little

/* This is the basic linkfollowing script.
 * Its pretty stable, and you configure which keys to use for hinting
 *
 * TODO: Some pages mess around a lot with the zIndex which
 * lets some hints in the background.
 * TODO: Some positions are not calculated correctly (mostly
 * because of uber-fancy-designed-webpages. Basic HTML and CSS
 * works good
 */


//Calculate element position to draw the hint
//Pretty accurate but on fails in some very fancy cases
function elementPosition(el) {
    var up = el.offsetTop;
    var left = el.offsetLeft;
    var width = el.offsetWidth;
    var height = el.offsetHeight;
    while (el.offsetParent) {
        el = el.offsetParent;
        up += el.offsetTop;
        left += el.offsetLeft;
    }
    return [up, left, width, height];
}
//Here we choose what to do with an element if we
//want to "follow" it. On form elements we "select"
//or pass the focus, on links we try to perform a click,
//but at least set the href of the link. (needs some improvements)
function clickElem(item, command) {
	midorator_command("delhints");
	midorator_command('hide entry');
	midorator_command(command, item);
}
//Draw all hints for all elements passed. "len" is for
//the number of chars we should use to avoid collisions
function reDrawHints(elems, chars) {
    midorator_command("delhints");
    for (var i = 0; i < elems[0].length; i++) {
        if (elems[0][i]) {
            var label = elems[1][i].substring(chars);
            var h = midorator_command('genhint', elems[0][i], label);
        }
    }
}
// pass: number of keys
// returns: key length
function labelLength(n) {
	var oldn = n;
	var keylen = 0;
	if(n < 2) {
		return 1;
	}
	n -= 1; // our highest key will be n-1
	while(n) {
		keylen += 1;
		n = Math.floor(n / charset.length);
	}
	return keylen;
}
// pass: number
// returns: label
function intToLabel(n) {
	var label = '';
	do {
		label = charset.charAt(n % charset.length) + label;
		n = Math.floor(n / charset.length);
	} while(n);
	return label;
}
// pass: label
// returns: number
function labelToInt(label) {
	var n = 0;
	var i;
	for(i = 0; i < label.length; ++i) {
		n *= charset.length;
		n += charset.indexOf(label[i]);
	}
	return n;
}
//Put it all together
function followLinks(follow, command) {
    var s = follow.split('');
    var linknr = labelToInt(follow);
    var elems = [ midorator_command('getelems', command), [] ];
    var len = labelLength(elems[0].length);
    var leftover = [[], []];
    if (s.length == len && linknr < elems[0].length && linknr >= 0) {
        clickElem(elems[0][linknr], command);
    } else {
        for (var j = 0; j < elems[0].length; j++) {
            var b = true;
            var label = intToLabel(j);
            var n = label.length;
            for (n; n < len; n++) {
                label = charset.charAt(0) + label;
            }
            for (var k = 0; k < s.length; k++) {
                b = b && label.charAt(k) == s[k];
            }
            if (b) {
                leftover[0].push(elems[0][j]);
                leftover[1].push(label);
            }
        }
        reDrawHints(leftover, s.length);
    }
}

var charset = '%s';
followLinks('%s', '%s');

