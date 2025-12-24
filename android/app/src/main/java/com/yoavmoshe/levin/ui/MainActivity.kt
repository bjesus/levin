package com.yoavmoshe.levin.ui

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import android.widget.TextView
import com.yoavmoshe.levin.R

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Temporary simple layout for initial testing
        val textView = TextView(this)
        textView.text = "Levin - Coming Soon"
        textView.textSize = 24f
        textView.setPadding(32, 32, 32, 32)
        
        setContentView(textView)
    }
}
