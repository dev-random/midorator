	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "cmdmap : entry :");
	midorator_process_command(web_view, "cmdmap <Tab> pass");
	midorator_process_command(web_view, "cmdmap ; entry ;");
	midorator_process_command(web_view, "cmdmap / entry /");
	midorator_process_command(web_view, "cmdmap ? entry ?");
	midorator_process_command(web_view, "cmdmap [[ go prev");
	midorator_process_command(web_view, "cmdmap ]] go next");
	midorator_process_command(web_view, "cmdmap r reload");
	midorator_process_command(web_view, "cmdmap R reload!");
	midorator_process_command(web_view, "cmdmap <space> scroll v + 1 p");
	midorator_process_command(web_view, "cmdmap <Page_Down> scroll v + 1 p");
	midorator_process_command(web_view, "cmdmap <Page_Up> scroll v - 1 p");
	midorator_process_command(web_view, "cmdmap <Up> scroll v - 1");
	midorator_process_command(web_view, "cmdmap k scroll v - 1");
	midorator_process_command(web_view, "cmdmap <Down> scroll v + 1");
	midorator_process_command(web_view, "cmdmap j scroll v + 1");
	midorator_process_command(web_view, "cmdmap <Home> scroll v = 0");
	midorator_process_command(web_view, "cmdmap gg scroll v = 0");
	midorator_process_command(web_view, "cmdnmap gg scroll v = %%i p");
	midorator_process_command(web_view, "cmdmap <End> scroll v = 32768 p");
	midorator_process_command(web_view, "cmdmap G scroll v = 32768 p");
	midorator_process_command(web_view, "cmdnmap G scroll v = %%i p");
	midorator_process_command(web_view, "cmdmap <BackSpace> go back");
	midorator_process_command(web_view, "cmdmap H go back");
	midorator_process_command(web_view, "cmdmap L go forth");
	midorator_process_command(web_view, "cmdmap p paste");
	midorator_process_command(web_view, "cmdmap P tabpaste");
	midorator_process_command(web_view, "cmdmap y yank");
	midorator_process_command(web_view, "cmdmap n next");
	midorator_process_command(web_view, "cmdmap N next!");
	midorator_process_command(web_view, "cmdmap t entry \":tabnew \"");
	midorator_process_command(web_view, "cmdmap o entry \":open \"");
	midorator_process_command(web_view, "cmdmap f entry ;f");
	midorator_process_command(web_view, "cmdmap F entry ;F");
	midorator_process_command(web_view, "cmdmap i insert");
	midorator_process_command(web_view, "cmdmap u undo");
	midorator_process_command(web_view, "cmdmap d q");
	midorator_process_command(web_view, "cmdmap gt action TabNext");
	midorator_process_command(web_view, "cmdnmap gt widget tabs page %%i");
	midorator_process_command(web_view, "cmdmap gT action TabPrevious");
	midorator_process_command(web_view, "cmdmap <CR> submit");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "cmdmap bm action Menubar");
	midorator_process_command(web_view, "cmdmap bn action Navigationbar");
	midorator_process_command(web_view, "cmdmap bb action Bookmarkbar");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "set hintstyle \"background-color: #59FF00; border: 2px solid #4A6600; color: black; font-size: 9px; line-height: 9px; font-weight: bold; margin: 0px; padding: 1px; z-index: 1000; border-radius: 6px;\"");
	midorator_process_command(web_view, "set hint_default \"a.href input select textarea button .onclick\"");
	midorator_process_command(web_view, "set hint_tabnew a.href");
	midorator_process_command(web_view, "set hint_yank \"a.href a.name\"");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "set go_next \"^>$, ^>>$, ^>>>$, ^next$, next *[>»], ^далее, ^след[.у], next, >\"");
	midorator_process_command(web_view, "set go_prev \"^<$, ^<<$, ^<<<$, ^prev$, ^previous, prev[.]? *[<«], previous *[<«], ^назад, ^пред[.ы], previous, <\"");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "");
	midorator_process_command(web_view, "source ~/.midoratorrc");
	midorator_process_command(web_view, "");
