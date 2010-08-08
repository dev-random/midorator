
function html_decode (s) {
	var from = new Array(/&gt;/g, /&lt;/g);
	var to = new Array(">", "<");
	for (var i = 0; i < from.length; i++) {
		s = s.replace(from[i], to[i]);
	}
	from = s.match(/&#[0-9]{1,5};/g);
	if (from)
		for (var i = 0; i < from.length; i++) {
			var n = parseInt(from[i].substr(2, from[i].length - 3));
			if (n)
				s = s.replace(new RegExp(from[i], 'g'), String.fromCharCode(n));
		}
	return s;
}



//Make onlick-links "clickable"
HTMLElement.prototype.click = function() {
	try {
		if (typeof this.onclick == 'function') {
			return this.onclick({
				type: 'click'
			});
		} else
			return true;
	} catch(e) {
		return true;
	}
};

var direction = '%s';
var links = document.links;

var re = new Array();

if (direction == 'next')
	re = new Array(/>/, />>/, />>>/, /^next$/i, /next *[>»]/i, /^далее/i, /^след[.у]/i, /next/i);
else if (direction == 'prev')
	re = new Array(/</, /<</, /<<</, /^prev$/i, /^previous/i, /prev[.]? *[>»]/i, /previous *[>»]/i, /^назад/i, /^пред[.ы]/i, /previous/i);

function go() {
	for (var r = 0; r < re.length; r++) {
		var ri = re[r];
		for (var l = 0; l < links.length; l++) {
			var li = links[l];
			if (html_decode(li.innerHTML.replace(/<[^>]*>/g, '')).match(ri) != null) {
				li.focus();
				if (li.click() != false)
					document.location = li.href;
				return;
			}
		}
	}
}

go();


