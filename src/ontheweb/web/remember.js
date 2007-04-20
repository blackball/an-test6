// adapted from http://www.quirksmode.org/js/cookies.html

function rememberMe(name,email) {
    document.cookie = "uname="+name+";path=/;";
    document.cookie = "email="+email+";path=/;";
}

function whoAmI() {
    var t = document.cookie.split(";");
    var count = 0;
    for(var i=0; i<t.length; i++){
        var n = t[i].split("=");
        n[0] = n[0].replace(/^ /g,""); //remove whitespace
        if (n[0]=="uname" || n[0]=="email") {
            count++;
            document.getElementById(n[0]).value = escape(n[1]);
        }

    }	
    if(count==2){
        document.getElementById("remember").checked = true;
    }
}

function checkRemember(){
    if(document.getElementById("remember").checked){
        rememberMe(document.getElementById("uname").value,
                   document.getElementById("email").value);
    } else {
        forgetMe();
    }

}

function forgetMe(){
    var exp = new Date();
    exp.setTime(exp.getTime()-1000*60*60*24);
    document.cookie = 'uname=""; expires='+exp.toGMTString()+";path=/;";
    document.cookie = 'email=""; expires='+exp.toGMTString()+";path=/;";
}
