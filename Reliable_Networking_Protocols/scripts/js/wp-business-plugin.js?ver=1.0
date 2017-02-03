jQuery(document).ready(function($){
	if( $(".ns-bp-faq-toggle .ns-bp-faq-toggle-title").hasClass('active') ){
		$(".ns-bp-faq-toggle .ns-bp-faq-toggle-title.active").closest('.ns-bp-faq-toggle').find('.ns-bp-faq-toggle-inner').show();
	}
	
	$(".ns-bp-faq-toggle .ns-bp-faq-toggle-title").click(function(){
		if( $(this).hasClass('active') ){
			$(this).removeClass("active").closest('.ns-bp-faq-toggle').find('.ns-bp-faq-toggle-inner').slideUp(200);
		}
		else{
			$(this).addClass("active").closest('.ns-bp-faq-toggle').find('.ns-bp-faq-toggle-inner').slideDown(200);
		}
	});
	jQuery('.nsbp-skillbar').each(function(){
		jQuery(this).find('.nsbp-skillbar-bar').animate({
			width:jQuery(this).attr('data-percent')
		},6000);
	});
	jQuery('.nsbp-testimonial-thumb-images img').hover(function(){
		var hoverclass=jQuery(this).attr('class');
		jQuery('.nsbp-testimonial-thumbnail-box p').hide();
		jQuery('.nsbp-testimonial-thumbnail-box blockquote').hide();
		jQuery('p.'+hoverclass).show(1000);
		jQuery('blockquote.'+hoverclass).show(1000);
	});
});
jQuery(function ($) {
  $("#ns_bp_totop")
    .css({position:'fixed',bottom:'20px'})
    .hide();
  $(window).scroll(function () {
    if ($(this).scrollTop() > 400) {
      $('#ns_bp_totop').fadeIn(200);
    } else {
      $('#ns_bp_totop').fadeOut(200);
    }
  });
  $('.nsbp-go-top').click(function () {
    $('html,body').animate({
      scrollTop: 0
    }, 1000);
    return false;
  });
});