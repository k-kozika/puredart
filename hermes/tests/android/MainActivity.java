package org.purehermes.test;
import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;
import java.io.*;
public class MainActivity extends Activity {
    static { com.facebook.soloader.nativeloader.NativeLoader.initIfUninitialized(new com.facebook.soloader.nativeloader.SystemDelegate()); System.loadLibrary("fbjni"); System.loadLibrary("test"); }
    static native String run(String path);
    @Override public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        TextView view = new TextView(this); setContentView(view);
        try {
            File file = new File(getFilesDir(), "fixture.hbc");
            try(InputStream input=getAssets().open("fixture.hbc"); OutputStream output=new FileOutputStream(file)) {
                byte[] bytes=new byte[8192];int n;while((n=input.read(bytes))!=-1)output.write(bytes,0,n);
            }
            String result=run(file.getAbsolutePath());
            android.util.Log.i("PurehermesTest", result); view.setText(result);
        } catch(Exception error) { view.setText(error.toString()); android.util.Log.e("PurehermesTest","FAIL",error); }
    }
}
